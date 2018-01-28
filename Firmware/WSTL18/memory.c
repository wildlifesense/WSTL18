/********************************************************************************

 memory.c

	This module was written for Atmel's ATmega328PB using Adesto's AT25DN512C
	512kbit SPI serial EEPROM. It was designed for memory and power optimization
	and	does not contain generalizations.

	Temperature logs consist of a settings byte, a timestamp, and a string
	of two-byte temperatures. Logs always begin at the first byte of a
	memory page, so only the first two [FEW?] bytes of each page need to be read
	to locate a start signature. Logs continue on through as many memory
	pages as necessary until the log stops or the memory array is full.
	There is no log end signature, as the log may terminate abruptly due to
	battery failure, corrosion, or a stop command via UART from the user.
	Although not recommended, the user can choose to begin a new log
	without erasing existing logs on the memory. In such a case, the system
	will locate the end of the last log in memory, skip any remaining bytes
	on the last memory page used, and begin a new log at the first byte of
	the next memory page.  The system does not allow the deletion of
	individual logs; this would cause fragmentation of the memory array and
	would require larger firmware programs to be handled. In stead, this
	system is aimed for simplicity.

	Author/Copyright: Nikos Vallianos (C) 2017.

	This module was developed for the WSTL18 temperature logger.

	Settings byte:
		Interval:

 
  Copyright (C) 2017 Nikos Vallianos <nikos@wildlifesense.com>
 
  This file is part of WSTL18 firmware.
 
  WSTL18 firmware is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  WSTL18 firmware is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  in the file COPYING along with this program.  If not,
  see <http://www.gnu.org/licenses/>.

********************************************************************************/
#include <avr/io.h>
#include <util/delay.h>
#include "spi.h"
#include "memory.h"
#include "uart.h"		// Debugging use only
#include "wstl18.h"


// Flash parameters
#define MEMORY_ARRAY_ADDRESS_FIRST		0x000000	// Address of first byte in memory array. Can be moved up to make room for settings.
#define MEMORY_ARRAY_ADDRESS_LAST		0x00FFFF	// Address of last byte in memory array.
#define MEMORY_PAGE_FIRST				0x00		// Number of first page in memory array used.
#define MEMORY_PAGE_LAST				0xFF		// Number of last page in memory array used.
#define MEMORY_PAGE_SIZE_BYTES			0xFF

#define MEMORY_GET_BYTE(ADDRESS)		(0xFF & ADDRESS)			// Return byte within page, 0x00-0xFF
#define MEMORY_GET_PAGE(ADDRESS)		((0xFF00 & ADDRESS) >> 8)	// Return page, 0x00-0xFF

#define MEMORY_BUSY_LIMIT	1000				// "Busy" status time read before flagging. 
uint16_t memory_next_log_location;				// Where will the next temperature log be stored? Should be even number.
uint8_t memory_status_byte_1;					// Every time Status Register 1 is read, it's also saved here.
uint8_t memory_status_byte_2;					// Every time Status Register 2 is read, it's also saved here.




// Read commands
//													address	Dummy	Data	MHz
#define	MEMORY_READ_ARRAY_FAST		0x0B		//	3		1		1+		104
#define	MEMORY_READ_ARRAY_SLOW		0x03		//	3		0		1+		33

// Erase
#define MEMORY_ERASE_PAGE			0x81		//	3		0		0		104
#define	MEMORY_ERASE_BLOCK_4K		0x20		//	3		0		0		104
#define	MEMORY_ERASE_BLOCK_32K		0x52		//	3		0		0		104
#define MEMORY_ERASE_BLOCK_32K_ALT	0xD8		//	3		0		0		104
#define MEMORY_ERASE_CHIP_1			0x60		//	0		0		0		104
#define MEMORY_ERASE_CHIP_2			0xC7		//	0		0		0		104
#define MEMORY_ERASE_CHIP_3_OLD		0x62		//	0		0		0		104	Legacy command, do not use.

// Byte / Page Program, 1 to 256 bytes
#define MEMORY_PROGRAM				0x02		//	3		0		1+		104		1 to 256 bytes, page overrun rewinds to start!

#define MEMORY_WRITE_ENABLE			0x06		//	0		0		0		104
#define MEMORY_WRITE_DISABLE		0x04		//	0		0		0		104

// Security register
#define MEMORY_PROGRAM_OTP_SECURITY_REGISTER	0x9B		//	3		0		1+		104
#define MEMORY_READ_OTP_SECURITY_REGISTER		0x77		//	3		2		1+		104

// Status

#define MEMORY_READ_STATUS_REGISTER				0x05		//	0		0		1+		104
#define MEMORY_WRITE_STATUS_REGISTER_BYTE_1		0x01		//	0		0		1		104
#define MEMORY_WRITE_STATUS_REGISTER_BYTE_2		0x31		//	0		0		1		104

// Device ID
#define MEMORY_RESET							0xF0		//	0		0		1(0xD0)	104		0xD0 confirms reset.
#define MEMORY_CONFIRM_RESET					0xD0		//	0		0		0		104
#define MEMORY_READ_MANUFACTURER_AND_DEVICE_ID	0x9F		//	0		0		1-4		104
#define MEMORY_READ_ID_LEGACY					0x15		//	0		0		2		104		Legacy

// Power down
#define MEMORY_DEEP_POWER_DOWN					0xB9		//	0		0		0		104
#define MEMORY_RESUME_FROM_DEEP_POWER_DOWN		0xAB		//	0		0		0		104
#define MEMORY_ULTRA_DEEP_POWER_DOWN			0x79		//	0		0		0		104

// Status Register 1
#define MEMORY_SR1_BPL		7		// [R/W] Block protection locked. 0: BP0 bit unlocked (def). 1: BP0 locked in current state when _WP_ asserted.
// Bit 6 reserved for future use.
#define MEMORY_SR1_EPE		5		// [R] 0: Erase or program operation was successful. 1: Erase or program error detected.
#define MEMORY_SR1_WPP		4		// [R] 0: _WP_ is asserted. 1: _WP_ is deasserted.
// Bit 3 reserved for future use.
#define MEMORY_SR1_BP0		2		// [R/W] 0: Entire memory array is unprotected. 1: Entire memory array is protected.
#define MEMORY_SR1_WEL		1		// [R] 0: Device is not write enabled (default). 1: Device is write enabled.
#define MEMORY_SR1_BSY		0		// [R] 0: Device is ready. 1: Device is busy with an internal operation.

// Status Register 2
// Bit 7 reserved for future use.
// Bit 6 reserved for future use.
// Bit 5 reserved for future use.
#define MEMORY_SR2_RSTE		4		// [R/W] 0: Reset command is disabled (default). 1: Reset command is enabled.
// Bit 3 reserved for future use.
// Bit 2 reserved for future use.
// Bit 1 reserved for future use.
#define MEMORY_SR2_BSY		0		// [R] 0: Device is ready. 1: Device is busy with an internal operation.

// Timing definitions from AT25DN512C datasheet.
//							usec
#define MEMORY_TIME_EDPD	2		// Maximum Chip Select high to deep power-down.
#define MEMORY_TIME_EUDPD	3		// Maximum Chip Select high to ultra-deep power-down (*)
#define MEMORY_TIME_SWRST	50		// Maximum software reset time.
#define MEMORY_TIME_CSLU	0.02	// Minimum Chip Select low to exit ultra-deep power-down
#define MEMORY_TIME_XUDPD	70		// Minimum exit ultra-deep power-down time
#define MEMORY_TIME_RDPD	8		// Maximum Chip Select high to standby mode. (from Deep power-down)

#define MEMORY_TIME_PP		1750	// Maximum Page Program time (256 Bytes)
#define MEMORY_TIME_BP		8		// Typical byte program time
#define MEMORY_TIME_PE		20000	// Maximum page erase time
#define MEMORY_TIME_BLKE4	50000	// Maximum 4Kbytes block erase time
#define MEMORY_TIME_BLKE32	350000	// Maximum 32Kbytes block erase time
#define MEMORY_TIME_CHPE	700000	// Maximum Chip Erase time
#define MEMORY_TIME_OTPP	950		// Maximum OTP Security Register program time
#define MEMORY_TIME_WRSR	40000	// Maxirum Write Status Register time
#define MEMORY_TIME_VCSL	70		// Minimum Vcc to Chip Select Low time
#define MEMORY_TIME_PUW		5000	// Max Power-up device delay before Program or Erase Allowed
// Also, device will perform a Power-On-Reset at 2.2V.



uint8_t memory_flags;						// Memory-related flags set by this module. See MEMORY_FLAG_xxxxx definitions below.

// Bits in this module's memory_flags variable and what they mean.
#define MEMORY_FLAG_FULL			7		// Memory is full.
#define MEMORY_FLAG_PROGRAM_ERROR	6		// Program error occurred on memory chip.
#define MEMORY_FLAG_BUSY_TIMEOUT	5		// Memory was busy for longer than timeout.
#define MEMORY_FLAG_OVERWRITE		4		// Recorded a temperature log on memory that wasn't erased (0xFF).
#define MEMORY_FLAG_FIRMWARE_ERROR	3		// Some sort of firmware error.
#define MEMORY_FLAG_2				2		// Something even different
#define MEMORY_FLAG_1				1		// Even more
#define MEMORY_FLAG_0				0		// Last


/*************************** Internal Functions *******************************/

/*
 * _memorySingleCommand: Send a single opcode to the memory chip.
 */
void _memorySingleCommand(uint8_t command) {
	MEMORY_CS_SELECT;
	spiTradeByte(command);
	MEMORY_CS_DESELECT;
}


/*
 *  _memorySendAddress: Send a 3-byte address to the flash unit. This function takes a
 *	two-byte input and adds the third 0x00 since that's always 0x00 on this module.
 */
void _memorySendAddress(uint16_t address) {
	uint8_t address_page = (uint8_t)(address>>8);
	uint8_t address_byte = (uint8_t) address;
	spiTradeByte(0x00);								// Send most significant byte first.
	spiTradeByte(address_page);						// Then send middle significant byte.
	spiTradeByte(address_byte);						// Then send least significant byte.
}


/*
 *	_memorySendDummy: Send dummy_size zeros over to the flash memory.
 */
void _memorySendDummy(uint8_t dummy_size) {
	for (uint8_t i=0; i<dummy_size; ++i) {
		spiTradeByte(0);
	}
}

/*
 *	_memoryFlagSet: Set the flag to indicate said error occurred.
 *	This is only for internal module use and has nothing to do
 *	with the actual memory chip.
 */
void _memoryFlagSet(uint8_t flag_position) {
	memory_flags |= (1<<flag_position);
}

/*
 * _memoryFlagClear: Unset the flag to indicate said error didn't occur or was fixed.
 */
void _memoryFlagClear(uint8_t flag_position) {
	memory_flags &= ~(1<<flag_position);
}

/*
 * _memoryFlagClearAll: Clear all flags to set module in reset state.
 */
void _memoryFlagClearAll(void) {
	memory_flags = 0;
}

/*********************** End Internal Functions *******************************/


/*********************** Begin Public Functions *******************************/

/*
	memoryInit: Initialize the memory chip for first use after power-up and until
	the logger is power-cycled.
 */
void memoryInitialize(void) {
	MEMORY_CS_INIT;						// Set memory pin as output.
	MEMORY_CS_DESELECT;					// Init but don't assert yet.
	
	memory_flags = 0;					// Clear memory flags.
	memory_next_log_location = 0;		// Replace this with a memory scan. Power-cycle shouldn't overwrite existing records.
	memory_status_byte_1 = 0;			// Replace this with reading status register.
	memory_status_byte_2 = 0;			// //--//
	// A bit more to do here.
}

/*
 * memoryTerminate: Stop logging and tie any loose ends on the currently
 * recorded log. Could cap the log with an end signature.
 * Used when the system detects a low battery power that would
 * cause the temperature sensor to malfunction. 
 *
 * memoryTerminate may not be a very good name for this function.
// Don't release CS pin, it will cause memory to possibly wake up from ultra-deep-power-down.
 */
void memoryTerminate(void) {
	; // Nothing to do here, yet.
}


/*
	memoryScan: Iterates through each memory page for these things:
		Does the memory contain previous data?
		Are previous records complete? (With valid starting and ending markers)
		How many previous records are there?
		Is the memory full? How many pages are available?
		
	This is useful in case of an unexpected power-cycle.
	
 */
uint8_t memoryScan(uint8_t starting_page) {
	uint8_t next_empty_page;
	uint8_t memory_array_byte;
	if (starting_page < MEMORY_PAGE_FIRST) {		// Prevent scanning before fist used page.
		starting_page = MEMORY_PAGE_FIRST;
	}
	if (starting_page >= 0)
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_ARRAY_SLOW);
	_memorySendAddress(starting_page);
	// Start reading from starting_spot
	while(starting_page <= MEMORY_ARRAY_ADDRESS_LAST) {
		memory_array_byte = spiTradeByte(0);
		if (memory_array_byte != 0xFF) {
			next_empty_page = starting_page;
		}
		++starting_page;
	}
	return(next_empty_page);
}


/*
 *	memoryReadStatusRegisters: Read the status registers of the memory chip
 *	and store them in this module's variables.
 */
void memoryReadStatusRegisters(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_STATUS_REGISTER);
	memory_status_byte_1 = spiTradeByte(0x00);
	memory_status_byte_2 = spiTradeByte(0x00);
	MEMORY_CS_DESELECT;
}

uint8_t memoryGetStatusRegister1(void) {
	return memory_status_byte_1;
}
uint8_t memoryGetStatusRegister2(void) {
	return memory_status_byte_2;
}
/*
 * Only two bits can be modified in register 1. The BPL bit regards the _WP_ pin, which
 * the WSTL18 doesn't use, so only the PB0 (block protection) bit could be modified. But
 * it probably won't, so this function will probably not be used.
 */
void memoryWriteStatusRegister1(uint8_t status) {
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_WRITE_STATUS_REGISTER_BYTE_1);
	spiTradeByte(status);
	MEMORY_CS_DESELECT;
	_memorySingleCommand(MEMORY_WRITE_DISABLE);
}

/*
 * Only the RSTE (reset enabled) bit can be modified in register 2.
 */
void memoryWriteStatusRegister2(uint8_t status) {
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_WRITE_STATUS_REGISTER_BYTE_2);
	spiTradeByte(status);
	MEMORY_CS_DESELECT;
	_memorySingleCommand(MEMORY_WRITE_DISABLE);
}

/*
	memoryBusy: returns 1 if memory is busy, 0 if it is available.
	There should probably be one function that retrieves regiter and checks,
	and one that checkes already retrieved register.
 */
_Bool memoryBusy(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_STATUS_REGISTER);
	memory_status_byte_1 = spiTradeByte(MEMORY_READ_STATUS_REGISTER);		// Does this work?
	MEMORY_CS_DESELECT;
	return (memory_status_byte_1 & (1<<MEMORY_SR1_BSY));
}

/*
 * Entering the Ultra-Deep Power-Down mode is accomplished by simply asserting the CS pin,
 * clocking in the opcode 79h, and then deasserting the CS pin. (AT25DN512C DS p.25).
 * When the CS pin is deasserted, the device will enter the ultra-deep power-down mode
 * within the maximum time of tEUDPD. The ultra-deep power-down command will be ignored if
 * an internally self-timed operation such as a program or erase cycle is in progress.
 */
void memoryUltraDeepPowerDownEnter() {
	spiEnable();
	_memorySingleCommand(MEMORY_ULTRA_DEEP_POWER_DOWN);
	spiDisable();
}


/*
 *	memoryUltraDeepPowerDownExit: Exit ultra deep power down mode.
 *
 *Master must wait for tXUDPD (70us) before sending another command,
 * or risk that command being ignored.
 * "When the CS pin is deasserted, the device will exit the Deep Power-Down
 * mode within the maximum time of tRDPD and return to the standby mode."

//#define MEMORY_TIME_CSLU	0.02	// Minimum Chip Select low to exit ultra-deep power-down
//#define MEMORY_TIME_XUDPD	70		// Minimum exit ultra-deep power-down time
 */
void memoryUltraDeepPowerDownExit(void) {
	MEMORY_CS_SELECT;
	_delay_us(1);				// Much greater than tCSLU but can't do otherwise.
	MEMORY_CS_DESELECT;
	_delay_us(100);				// tXUDPD, unless the device can do something else in this time.
}

/*
 * _memoryWriteEnable: Set the Write Enable Latch (WEL) in status register to
 * allow page/byte program, erase, program OTP security register and Write Status
 * Register commands to write to the memory.
 */
void _memoryWriteEnable(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_WRITE_ENABLE);
	MEMORY_CS_DESELECT;
}

/*
 * _memoryWriteDisable: Clear the Write Enable Latch (WEL) in status register to
 * prevent page/byte program, erase, program OTP security register, and Write Status
 * Register commands from writing to the memory.
 */
void _memoryWriteDisable(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_WRITE_DISABLE);
	MEMORY_CS_DESELECT;
}


// Bytes 0-63 are user one-time programmable.
// OTP writing wraps over if starting after 0 and exceeding past 63. Only last 64 bytes are written.
// Unprogrammed state is 0xFF.
void _memoryOTPWrite(void) {
	_memoryWriteEnable();
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_PROGRAM_OTP_SECURITY_REGISTER);
	// 3-byte address where writing will begin. A23-A6 are ignored, A5-A0 denote number 0 to 63.
	spiTradeByte(0x00);		// OTP Address byte 1 (A23-A16)
	spiTradeByte(0x00);		// OTP Address byte 2 (A15-A8)
	spiTradeByte(0x00);		// OTP Address byte 3 (A7-A0). A5-A0 actually count, 0 starts at the beginning.
	/// ... 1 to 64 bytes of data.
	MEMORY_CS_DESELECT;
	// _delay_us(950);
}
#define MEMORY_OTP_ARRAY_LENGTH 128
uint8_t memory_OTP_array[MEMORY_OTP_ARRAY_LENGTH];

/*
 * Read the entire OTP register. Data are loaded to module array.
 */
void memoryOTPLoad(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_OTP_SECURITY_REGISTER);
	spiTradeByte(0x00);		// Three address bytes
	spiTradeByte(0x00);		// Of course we start ...
	spiTradeByte(0x00);		// ... at start of array.
	spiTradeByte(0x00);		// ... also send two dummy bytes
	spiTradeByte(0x00);		// ... as required ...
	// ... then read the entire array, including unique number (bytes 64 to 127).
	for(uint8_t i=0; i<MEMORY_OTP_ARRAY_LENGTH; i++) {
		memory_OTP_array[i] = spiTradeByte(0xFF);
	}
	MEMORY_CS_DESELECT;
}


void memoryOTPPrint(void) {
	uartEnable();
	for(uint8_t i=0; i<MEMORY_OTP_ARRAY_LENGTH; i++) {
		uartSendByte(memory_OTP_array[i]);
	}
	uartDisable();
}
void memoryWriteTwoBytes(void) {

}

/*
 * memoryLogTemperature:
 *		This function also checks that the device is not busy
 *		Takes a 16-bit temperature and writes it at the next location on memory.
 *		at the end advances the next location pointer by two positions.
 *		After writing the data, this function checks the device's status registers to ensure the write was successful.
 *		Optionally can check that the current byte is an 0xFF and flag if it isn't.
 *
 *	Returns:
 *		TODO: What should this return?
 */
uint8_t memoryLogTemperature(uint16_t temperature_reading) {
	uint16_t memory_busy_counter = 0;
	while(memoryBusy() && (memory_busy_counter < MEMORY_BUSY_LIMIT)) {	// Wait until memory is not busy.
		++memory_busy_counter;
	}
	if (memory_busy_counter >= MEMORY_BUSY_LIMIT) {
		_memoryFlagSet(MEMORY_FLAG_BUSY_TIMEOUT);
		// TODO: If memory keeps being busy, the log should stop.
		return 0;
		} else {	// Write data to memory.
		uint8_t data_msb = (uint8_t) (temperature_reading>>8);
		uint8_t data_lsb = (uint8_t) temperature_reading;
		_memorySingleCommand(MEMORY_WRITE_ENABLE);
		// Turn the following into a function
		MEMORY_CS_SELECT;
		spiTradeByte(MEMORY_PROGRAM);
		_memorySendAddress(0);//write_location); // TODO: Fix this mess
		// No dummy needed for writing.
		spiTradeByte((uint8_t) data_msb);
		spiTradeByte((uint8_t) data_lsb);
		MEMORY_CS_DESELECT;
		_memorySingleCommand(MEMORY_WRITE_DISABLE);		// (Nikos) This may be unnecessary if chip does it automatically. Check.
		return 1;
	}
	


	if (memory_next_log_location >= (MEMORY_ARRAY_ADDRESS_LAST-1)) {
		_memoryFlagSet(MEMORY_FLAG_FULL);
	} else {
		memory_next_log_location += 2;			// TODO: Caution: 16-bit number may loop to 0.

	}
	return 1;
}

uint8_t memory_MDID[12];

void memoryReadMFDID(void) {

	spiEnable();
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_MANUFACTURER_AND_DEVICE_ID);
	for(uint8_t i=0; i<12; i++) {
		memory_MDID[i] = spiTradeByte(0x00);
		if( !memory_MDID[i]) {
			break;
		}
	}
	MEMORY_CS_DESELECT;
	spiDisable();
}

void memoryPrintMFDID(void) {
	uartEnable();
	for (uint8_t i=0; i<12; i++) {
		uartSendByte(memory_MDID[i]);
		if( !memory_MDID[i]) {
			break;
		}
	}
	uartDisable();

}