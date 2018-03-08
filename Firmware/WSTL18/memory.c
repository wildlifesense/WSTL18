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
#include "uart.h"		// Debugging use only
#include "memory.h"


// Flash parameters
#define MEMORY_ARRAY_ADDRESS_FIRST		0x000000	// Address of first byte in memory array. Can be moved up to make room for settings.
#define MEMORY_ARRAY_ADDRESS_LAST		0x00FFFF	// Address of last byte in memory array.
#define MEMORY_PAGE_FIRST				0x00		// Number of first page in memory array used.
#define MEMORY_PAGE_LAST				0xFF		// Number of last page in memory array used.
#define MEMORY_PAGE_SIZE				256			// Number of bytes in a memory page.

#define MEMORY_GET_BYTE(ADDRESS)		(0xFF & ADDRESS)			// Return byte within page, 0x00-0xFF
#define MEMORY_GET_PAGE(ADDRESS)		((0xFF00 & ADDRESS) >> 8)	// Return page, 0x00-0xFF

#define MEMORY_BUSY_LIMIT	1000				// "Busy" status time read before flagging. 
uint8_t memory_status_byte_1;					// Every time Status Register 1 is read, it's also saved here.
uint8_t memory_status_byte_2;					// Every time Status Register 2 is read, it's also saved here.
uint8_t memory_manufacturer_id;					// First byte output after "read manufacturer and device id" opcode is clocked in.
uint16_t memory_device_id;						// Subsequend two bytes output after "read manufacturer and device id" opcode is clocked in.

#define MEMORY_OTP_ARRAY_LENGTH 128
uint8_t memory_OTP_array[MEMORY_OTP_ARRAY_LENGTH];

// Read commands
//													address	Dummy	Data	MHz
#define	MEMORY_READ_ARRAY_FAST		0x0B		//	3		1		1+		104
#define	MEMORY_READ_ARRAY_SLOW		0x03		//	3		0		1+		33

// Erase
#define MEMORY_ERASE_PAGE			0x81		//	3		0		0		104 Time 6-20ms.
#define	MEMORY_ERASE_BLOCK_4K		0x20		//	3		0		0		104 Time 35-50ms.
#define	MEMORY_ERASE_BLOCK_32K		0x52		//	3		0		0		104 Time 250-350ms.
#define MEMORY_ERASE_BLOCK_32K_ALT	0xD8		//	3		0		0		104 Time 250-350ms.
#define MEMORY_ERASE_CHIP_1			0x60		//	0		0		0		104 Time 500-700ms.
#define MEMORY_ERASE_CHIP_2			0xC7		//	0		0		0		104 Time 500-700ms.
#define MEMORY_ERASE_CHIP_3_OLD		0x62		//	0		0		0		104	Time 500-700ms. Legacy command, do not use.

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
 * Send a single opcode to the memory chip. Works with all commands
 * that don't require any address or dummy bytes and don't return
 * any data back (write enable/disable and power down commands).
 */
void _memorySingleCommand(uint8_t command) {
	MEMORY_CS_SELECT;
	spiTradeByte(command);
	MEMORY_CS_DESELECT;
}

/*
 *  Send a 3-byte address to the flash unit. This function takes a
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
 * Send dummy_size zeros over to the flash memory.
 */
void _memorySendDummy(uint8_t dummy_size) {
	for (uint8_t i=0; i<dummy_size; ++i) {
		spiTradeByte(0);
	}
}


/*
 *	Set the flag to indicate said error occurred.
 *	This is only for internal module use and has nothing to do
 *	with the actual memory chip.
 */
void _memoryFlagSet(uint8_t flag_position) {
	memory_flags |= (1<<flag_position);
}

/*
 * Unset the flag to indicate said error didn't occur or was addressed.
 */
void _memoryFlagClear(uint8_t flag_position) {
	memory_flags &= ~(1<<flag_position);
}

/*
 * Clear all flags to set module flags in reset state.
 */
void _memoryFlagClearAll(void) {
	memory_flags = 0;
}

/*
 * Check if memory chip is busy with a program or erase.
 * Returns 1 if memory is busy, 0 if it is available.
 */
uint8_t _memoryBusy(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_STATUS_REGISTER);
	memory_status_byte_1 = spiTradeByte(MEMORY_READ_STATUS_REGISTER);		// Does this work?
	MEMORY_CS_DESELECT;
	return (memory_status_byte_1 & (1<<MEMORY_SR1_BSY));
}
/*
 * Check if memory chip is busy and wait for a bit. If wait is too long, register
 * relevant flag and stop waiting.
 * This function should be used at top of every program/erase function.
 */
void _memoryCheckBusy(void) {
	uint16_t memory_busy_counter = 0;
	while(_memoryBusy() && (memory_busy_counter < MEMORY_BUSY_LIMIT)) {	// Wait until memory is not busy.
		++memory_busy_counter;
		_delay_us(10);
	}
	if (memory_busy_counter >= MEMORY_BUSY_LIMIT) {
		_memoryFlagSet(MEMORY_FLAG_BUSY_TIMEOUT);
	}
}

/*
 * Enable a reset operation by setting the RSTE bit in status register 2.
 */
void _memoryResetEnable(void) {
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_WRITE_STATUS_REGISTER_BYTE_2);
	spiTradeByte(1<<MEMORY_SR2_RSTE);
	MEMORY_CS_DESELECT;
	_memorySingleCommand(MEMORY_WRITE_DISABLE);
}


/*
 * Do a memory operation reset while memory is busy.
 * This stops any program or erase operation currently in progress.
 * The contents of the page currently being written cannot be guaranteed
 * after a reset operation.
 */
void _memoryResetOperation(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_RESET);
	spiTradeByte(MEMORY_CONFIRM_RESET);
	MEMORY_CS_DESELECT;
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
	spiEnable();
	memoryReadStatusRegisters();		// Read memory chip status registers into module.
	memoryReadMFDID();					// Read memory chip manufacturer and device ids.
	//memoryScan();	???
	spiDisable();
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
 * Write a byte onto the memory chip
 */
void memoryWriteByte(uint16_t array_address, uint8_t written_byte) {
	_memoryCheckBusy();
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_PROGRAM);
	_memorySendAddress(array_address);
	spiTradeByte(written_byte);
	MEMORY_CS_DESELECT;
}

/*
 * Read a byte from memory chip.
 */
uint8_t memoryReadByte(uint16_t array_address) {
	uint8_t returned_byte;
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_ARRAY_SLOW);
	_memorySendAddress(array_address);
	returned_byte = spiTradeByte(0x00);
	MEMORY_CS_DESELECT;
	return returned_byte;
}

/*
 * Write a 16-bit value to memory starting (msb) at array_address.
 */
void memoryWriteWord(uint16_t array_address, uint16_t written_word) {
	_memoryCheckBusy();
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_PROGRAM);
	_memorySendAddress(array_address);
	// No dummy needed for writing.
	spiTradeByte((uint8_t) (uint8_t) (written_word>>8));
	spiTradeByte((uint8_t) written_word);
	MEMORY_CS_DESELECT;
}

/*
 * Read a 16-bit value from memory starting (msb) at array_address.
 */
uint16_t memoryReadWord(uint16_t array_address) {
	uint8_t word_msb;
	uint8_t word_lsb;
	uint16_t returned_word;
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_ARRAY_SLOW);
	_memorySendAddress(array_address);
	// Slow read doesn't need a dummy byte.
	word_msb = spiTradeByte(0x00);
	word_lsb = spiTradeByte(0x00);
	MEMORY_CS_DESELECT;
	returned_word = word_msb;
	returned_word <<= 8;		// Shift msb 8 bits left
	returned_word |= word_lsb;	// Or in the 8 lsb bits
	return returned_word;
}


/*
 * Read number_of_bytes bytes from array_pointer and write them to
 * EEPROM starting at eeprom_address.
 * This function doesn't check for page wrap.
 */
void memoryWriteArray(uint16_t starting_address, uint8_t *array_pointer, uint8_t number_of_bytes) {
	uint8_t address_byte = (uint8_t) starting_address & 0xFF;
	_memoryCheckBusy();
	// How many writes?
	if ( (MEMORY_PAGE_SIZE-number_of_bytes) >= address_byte) {	// All array fits in current page?
		_memorySingleCommand(MEMORY_WRITE_ENABLE);
		MEMORY_CS_SELECT;
		spiTradeByte(MEMORY_PROGRAM);
		_memorySendAddress(starting_address);
		for (uint8_t i=0; i<number_of_bytes; i++) {
			spiTradeByte(array_pointer[i]);
		}
		MEMORY_CS_DESELECT;
	} else {													// If not, we'll have to do two writes to avoid page wrap.
		// split array into two writes
	}
}


/*
 * Read number_of_bytes bytes from memory chip starting at address starting_address and store them
 * into array pointed by array_pointer.
 */
void memoryReadArray(uint16_t starting_address, uint8_t *array_pointer, uint8_t number_of_bytes) {
	_memoryCheckBusy();
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_ARRAY_SLOW);
	_memorySendAddress(starting_address);
	for (uint8_t i=0; i<number_of_bytes; i++) {
		array_pointer[i] = spiTradeByte(0x00);
	}
	MEMORY_CS_DESELECT;
}

void memoryDumpAll(void) {
	spiEnable();
	uartEnable();
	_memoryCheckBusy();
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_ARRAY_SLOW);
	_memorySendAddress(0x0000);
	uint16_t i=0;
	do {
		uartSendByte(spiTradeByte(0x00));
		i++;
	} while (i>0);
	MEMORY_CS_DESELECT;
	spiDisable();
	uartDisable();
}


/*
 * Scan memory array backwards and find the last byte that isn't blank (0xFF).
 * Returns the address of the last non-blank byte or 0 if memory array is all blank.
 *
 * *Special case note: If byte 0 of eeprom is the (first and) last non-blank byte,
 * any data that might represent is useless anyway, the memory array might just as
 * well be blank, so that special case isn't really a problem (though it shouldn't
 * really ever occur.
 */
uint16_t memoryScan(void) {
	uint16_t last_data_byte_address;
	uint8_t current_page = MEMORY_PAGE_LAST;
	uint8_t current_byte;
	uint8_t page_read_buffer[0xFF];		// 256 bytes == 1 eeprom page.
	spiEnable();
	// For each page counting down (0xFF to 0)
	do {
		current_byte = 0;
		MEMORY_CS_SELECT;
		spiTradeByte(MEMORY_READ_ARRAY_SLOW);
		spiTradeByte(0x00);						// Clock three-byte address ...
		spiTradeByte(current_page);				// ... of start of ...
		spiTradeByte(0x00);						// ... current page.
		
		// Read each byte of this page into buffer
		do {
			page_read_buffer[current_byte] = spiTradeByte(0x00);
			current_byte++;
		} while(current_byte);			// ... until current_byte wraps back to 0
		MEMORY_CS_DESELECT;
		
		// Check buffer backwards till we find a non-0xFF
		current_byte = 0xFF;	// 0xFF == 255
		do {
			if (page_read_buffer[current_byte] != 0xFF) {		// A non-0xFF byte was found ...
				spiDisable();
				last_data_byte_address = current_page;			// Produce its 16-bit address (page+byte)
				last_data_byte_address <<= 8;
				last_data_byte_address |= current_byte;
				return last_data_byte_address; // Is this correct? Check!
			}
			current_byte--;
		} while ( current_byte!=0xFF );	// Repeat until it wraps back to 0xFF.
		
		current_page--;
	} while( current_page!=0xFF ); // Repeat until page wraps back to 0xFF
	
	spiDisable();
	return 0;	// If all memory is blank, return 0.
}

/*
 * Erase EEPROM page page_number (0x00-0xFF). Erased state for each byte is 0xFF.
 */
void memoryErasePage(uint8_t page_number) {
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_ERASE_PAGE);
	spiTradeByte(0x00);						// Dummy
	spiTradeByte(page_number);
	spiTradeByte(0x00);						// Dummy
	MEMORY_CS_DESELECT;
	// Takes 6-20ms.
}

void memoryEraseChip(void) {
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_ERASE_CHIP_1);
	MEMORY_CS_DESELECT;
	// Takes 500-700ms
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
 * Exit ultra deep power down mode.
 * Master must wait for tXUDPD (70us) before sending another command,
 * or risk that command being ignored.
 * "When the CS pin is deasserted, the device will exit the Deep Power-Down
 * mode within the maximum time of tRDPD and return to the standby mode."

//#define MEMORY_TIME_CSLU	0.02	// Minimum Chip Select low to exit ultra-deep power-down
//#define MEMORY_TIME_XUDPD	70		// Minimum exit ultra-deep power-down time
 */
void memoryUltraDeepPowerDownExitBegin(void) {
	MEMORY_CS_SELECT;
	_delay_us(1);				// Much greater than tCSLU but can't do otherwise.
	MEMORY_CS_DESELECT;
}




// Bytes 0-63 are user one-time programmable.
// OTP writing wraps over if starting after 0 and exceeding past 63. Only last 64 bytes are written.
// Unprogrammed state is 0xFF.
void _memoryOTPWrite(void) {
	_memorySingleCommand(MEMORY_WRITE_ENABLE);
	
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_PROGRAM_OTP_SECURITY_REGISTER);
	// 3-byte address where writing will begin. A23-A6 are ignored, A5-A0 denote number 0 to 63.
	spiTradeByte(0x00);		// OTP Address byte 1 (A23-A16)
	spiTradeByte(0x00);		// OTP Address byte 2 (A15-A8)
	spiTradeByte(0x00);		// OTP Address byte 3 (A7-A0). A5-A0 actually count, 0 starts at the beginning.
	/// ... 1 to 64 bytes of data.
	MEMORY_CS_DESELECT;
	_memorySingleCommand(MEMORY_WRITE_DISABLE);
	// _delay_us(950);
}


/*
 * Load the entire OTP register into memory array.
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
	if(1) {
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
	

	// This is DUMMY, remove and fix/clear code below.
	//if (memory_next_log_location >= (MEMORY_ARRAY_ADDRESS_LAST-1)) {
		//_memoryFlagSet(MEMORY_FLAG_FULL);
	//} else {
		//memory_next_log_location += 2;			// TODO: Caution: 16-bit number may loop to 0.
	//}
	return 1;
}

void memoryReadMFDID(void) {
	MEMORY_CS_SELECT;
	spiTradeByte(MEMORY_READ_MANUFACTURER_AND_DEVICE_ID);
	memory_manufacturer_id = spiTradeByte(0x00);
	memory_device_id = spiTradeByte(0x00);
	memory_device_id <<= 8;
	memory_device_id |= spiTradeByte(0x00);
	MEMORY_CS_DESELECT;
}

/*			TESTING SECTION            */

/*
 * Run various routines that test that:
 *   1. The memory chip functions and is well connected with the MCU, and
 *   2. That this module's code functions as intended.
 */
void memoryRunTests(void) {
	// Initialize the chip...
	// Put the chip to sleep and wait (allows current measurement).
	// Read the chip's manufacturer and device ID (view results through debugger).
	// Read the chip's one-time programmable section.
	// Read the chip's eeprom array to verivy it's empty.
	// Write some data onto the chip's eeprom.
	// Test the isBusy function.
	// Find how full the memory chip is.
	// Read the data back to verify it's as expected.
	// Perform a chip erase.
	// Test the isBusy() function.
	// Verify the chip is empty again.
}