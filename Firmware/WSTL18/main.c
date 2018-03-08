/********************************************************************************

  main.c: The main routine of the NestProbe TL1 firmware.
 
  Copyright (C) 2017-2018 Nikos Vallianos <nikos@wildlifesense.com>
  
  This file is part of NestProbe TL1 firmware.
  
  NestProbe TL1 firmware is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  NestProbe TL1 firmware is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  in the file COPYING along with this program.  If not,
  see <http://www.gnu.org/licenses/>.

  ********************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define SIGRD 5					// reading ID may not work without this
#include <avr/boot.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>		// Used to read bytes from flash memory.
#include <util/crc16.h>			// Used to calculate firmware's CRC.
#include <util/delay.h>
#include "error.h"
#include "uart.h"
#include "memory.h"
#include "max30205.h"
#include "indicator.h"

#include "spi.h"

#define system_soft_reset() \
do                          \
{                           \
	wdt_enable(WDTO_15MS);  \
	for(;;)                 \
	{                       \
	}                       \
} while(0)


char     logger_status;
uint16_t logger_countdown;				// 8-second counts until logger begins recording temperatures.
uint16_t logger_interval;				// How many eight seconds counts till logging interval reached.
uint16_t logger_temperature;
uint16_t logger_memory_location;		// Initialize to 0 or not?
uint16_t logger_local_serial;
uint16_t logger_battery_level;
volatile uint16_t logger_eightseconds_count;		// Number of eightsecond counts since last log

#define FIRMWARE_VERSION_MAJOR 0		// 0-16 so it can squeeze into left-half of 8-bit value.
#define FIRMWARE_VERSION_MINOR 1		// 0-16 so it can squeeze into right-half of 8-bit value.
#define FIRMWARE_GET_VERSION ((uint8_t)(FIRMWARE_VERSION_MAJOR<<4|FIRMWARE_VERSION_MINOR))	// Get version in one byte.

#define LOGGER_HEADER_SIZE	22
#define LOGGER_FOOTER_SIZE	22
#define RX_RECEIVE_TIMEOUT	100					// Time window to receive command, in ms
#define RX_BUFFER_LENGTH	128
volatile uint8_t rx_buffer_index;						// Points to current writing location in buffer.
volatile uint8_t rx_buffer_array[RX_BUFFER_LENGTH];		// Buffer for bytes received through uart.

uint8_t mcu_id[3];						// Identifies a 328PB.
uint8_t mcu_serial_number[10];			// Unique serial number.

// MCU flash memory firmware and bootloader parameters.
uint16_t mcu_bootloader_first_byte;			// Byte address of first bootloader byte. Depends on BOOTSZ fuses.
uint16_t mcu_firmware_available_pages;		// Number of pages available for application firmware.
uint16_t mcu_firmware_last_byte;			// Byte address of last byte of current firmware. Helps calculate firmware size and CRC.
uint16_t mcu_firmware_crc16_xmodem;			// Xmodem-CRC16 of mcu firmware.
uint16_t last_data_byte;					// Last byte with data on memory chip. Stores scan at boot to know if there's resident data on memory chip.
void rxBufferClear(void);			// Clear rx buffer and index.
void hostCommandReceive(void);		// Send host info string and receive a command.


/*
 * Timer 2 is the Real Time Counter, set to fire every 8 seconds.
 */
ISR(TIMER2_OVF_vect) {
	logger_eightseconds_count++;
}



ISR(USART0_RX_vect) {
	if (rx_buffer_index<RX_BUFFER_LENGTH) {
		rx_buffer_array[rx_buffer_index] = UDR0;
		rx_buffer_index++;	
	} else {
		errorSetFlag(ERROR_FLAG_RX_BUFFER_OVF);
	}
}




/*
 * MAIN function, consists of initialization code and an eternal while(1) loop.
 */
int main(void) {
	// Initialize the system. So many things!
	MCUSR = 0;
	wdt_disable();			// Disable watchdog timer, in case this was preserved after soft-reset.
	// Disable brown-out detector in sleep mode.
	MCUCR |= (1<<BODSE)|(1<<BODS);
	MCUCR |= (1<<BODS);
	MCUCR &= ~(1<<BODSE);

	// Disable ADC
	ADCSRA &= ~(1<<ADEN);					// Disable ADC, stops the ADC clock.

	// TODO: Disable watchdog timer (Fuse FUSE_WDTON, defaults to ...) Also WDTCSR
	// TODO: Disconnect the bandgap reference from the Analog Comparator (clear the ACBG bit in	ACSR (ACSR.ACBG)).
	ACSR |= (1<<ACD);						// Disable analog comparator (set to disable).

	PRR0 |= (1<<PRTWI0)|(1<<PRTIM2)|(1<<PRTIM0)|(1<<PRUSART1)|(1<<PRTIM1)|(1<<PRSPI0)|(1<<PRUSART0)|(1<<PRADC);
	PRR1 |= (1<<PRTWI1)|(1<<PRPTC)|(1<<PRTIM4)|(1<<PRSPI1)|(1<<PRTIM3);

	// SM[2:0] = b'010' for power_down and b'011' for power_save
	//SMCR &= ~((1<<SM2)|(1<<SM0));
	//SMCR |= (1<<SM1);

	// Set all ports to input and high before initializing modules that may override these as necessary.
	// Turn all unused pins into inputs with pull-ups.
	DDRB  = 0x00;
	PORTB = 0xFF;
	DDRC  &= ~((1<<DDRC0)|(1<<DDRC1)|(1<<DDRC2)|(1<<DDRC3)|(1<<DDRC4)|(1<<DDRC5)|(1<<DDRC6));		// Port C is 7 bits (0-6).
	PORTC |= ((1<<PORTC0)|(1<<PORTC1)|(1<<PORTC2)|(1<<PORTC3)|(1<<PORTC4)|(1<<PORTC5)|(1<<PORTC6));
	DDRD  = 0x00;
	PORTD = 0xFF;
	DDRE  &= ~((1<<DDRE0)|(1<<DDRE1)|(1<<DDRE3));
	PORTE |= ((1<<PORTE0)|(1<<PORTE1)|(1<<PORTE3));
	DDRE |= (1<<DDRE2);
	PORTE &= ~(1<<PORTE2);					// Stays here
	
	
	//Digital input buffers can be disabled by writing to the Digital Input Disable Registers (DIDR0 for ADC, DIDR1 for AC). (found at http://microchipdeveloper.com/8avr:avrsleep)
	//If the On-chip debug system is enabled by the DWEN Fuse and the chip enters sleep mode, the main clock source is enabled and hence always consumes power. In the deeper sleep modes, this will contribute significantly to the total current consumption.

	errorInitFlags();
	indicatorInitialize();
	indicatorOn(); // Debug
	
	// Start the Real Time Counter. Takes 1000ms+ to allow crystal to stabilize.
	PRR0	&= ~(1<<PRTIM2);								// Clear timer2 bit at power reduction register.
	ASSR	|= (1<<AS2);									// Clock from external crystal.
	TCCR2B	|= (1<<CS20)|(1<<CS21)|(1<<CS22);				// Tosc/1024 prescaler = 8sec to overflow.
	TIMSK2	= 0;
	TIMSK2	|= (1<<TOIE2);									// Enable overflow interrupt
	//_delay_ms(1000);										// Allow RTC crystal to stabilize (RTC AN p.5).
	TCNT2	= 0;											// Clear counter value.
	sei();
	//interval_limit = 75;									// Default to 10 minutes.
	//rtcIntervalCounterClear();
	
	// Initialize temperature sensor chip into lowest power consumption.
	max30205Init();
	memoryInitialize();								// Initialize memory chip.
	last_data_byte = memoryScan();					// Find last memory chip address with data. Should be 0.
	memoryUltraDeepPowerDownEnter();				// Place memory chip into lowest power consumption.
	if (last_data_byte) {
		logger_status = 'H';
	} else {
		logger_status = 'I';
	}
	// Load MCU ID.
	mcu_id[0] = boot_signature_byte_get(0);
	mcu_id[1] = boot_signature_byte_get(2);
	mcu_id[2] = boot_signature_byte_get(4);

	// Load MCU serial number.
	for (uint8_t i=0; i<10; i++) {
		mcu_serial_number[i] = boot_signature_byte_get(i+14);	// Read signature bytes 14-23
	}
	logger_local_serial = 0;		// Read this from MCU EEPROM
	logger_battery_level = 0;		// Read the battery voltage (read bandgap with AVCC reference).
	// Load bootloader start address and firmware available pages
	uint8_t bootsz_bits = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
	bootsz_bits = bootsz_bits & 0b00000110;					// Keep only bootsz bits. 1: unseet, 0: set
	bootsz_bits = bootsz_bits >> 1;									// Move bootsz bits to base
	bootsz_bits = ~(bootsz_bits) & 0b11;							// Reverse bootsz bits and filter out other bits
	// Bootsz bits are now 0-3 representing 4, 8, 16, or 32 pages of boot sector.
	mcu_bootloader_first_byte = 0x8000 - (1<<bootsz_bits)*512;		// bootsector size = math.pow(2, bootsz) * 512
	mcu_firmware_available_pages = mcu_bootloader_first_byte / SPM_PAGESIZE;

	// Find the last byte of firmware in flash.
	for (uint16_t k=(mcu_bootloader_first_byte-1); k>0; k--) {
		if (pgm_read_byte(k) != 0xFF) {
			mcu_firmware_last_byte = k;
			break;
		}
	}

	// Calculate firmware xmodem CRC16
	mcu_firmware_crc16_xmodem = 0;
	for (uint16_t i=0; i<=mcu_firmware_last_byte; i++) {
		mcu_firmware_crc16_xmodem = _crc_xmodem_update(mcu_firmware_crc16_xmodem, pgm_read_byte(i));
	}

	// Set the initial logger status.
	//logger_status = 'I';
	
	_delay_ms(500);
	indicatorOff();
	set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	/*******************************************
	*               MAIN LOOP                  *
	*******************************************/
    while (1) {
		sleep_mode();
		if (logger_status == 'L') {
			if (logger_countdown>0) {
				logger_countdown--;
				logger_eightseconds_count = 0;
			} else if (logger_eightseconds_count<logger_interval) {
					// Do nothing
			} else {
				if (logger_memory_location < LOGGER_HEADER_SIZE) {	// Wrapped through end of memory?
					logger_status = 'H';
				} else {
					memoryUltraDeepPowerDownExitBegin();
					max30205StartOneShot();
					_delay_ms(50);										// Wait for MAX30205 to read temperature.
					logger_temperature = max30205ReadTemperature();
					_delay_ms(20);										// Wait for memory to recover from deep power down.
					memoryWriteWord(logger_memory_location, logger_temperature);
					logger_memory_location += 2;
					logger_eightseconds_count = 0;
					memoryUltraDeepPowerDownEnter();
				}
			}
		} else if (logger_status=='H') {
			; // Do nothing, for now
		} else if (logger_status=='I') {
			;
		} // End of status if-implemented 'switch'
		
		
		if ( !(PIND & (1<<PIND2))) { // PD2 is down. NOTE: PC3 is temporary for NestProbe TL1P1. Use PD2 for NestProbe TL1P2/P3
			hostCommandReceive();
		}
		indicatorShortBlink();
	}// END OF MAIN LOOP
}// END OF MAIN




/*
 * Clear the rx buffer and its index.
 */
void rxBufferClear(void) {
	for(uint8_t i=0; i<RX_BUFFER_LENGTH; i++) {
		rx_buffer_array[i] = 0;
	}
	rx_buffer_index = 0;
}

/*
 * Send host a string of info and receive a command.
 */
void hostCommandReceive(void) {

	// Send logger info string (4 bytes of model, 3 bytes of MCU ID (why?), 10 bytes of MCU serial number (OK), 2 bytes of firmware CRC, 6 status-dependent bytes.
	rxBufferClear();									// Clear rx_buffer_array and rx_buffer_index.
	uartEnable();										// Enable uart module.
	uartRxInterruptEnable();							// Enable RX receive interrupt.

	uartSendString("\0TL1");							// 4 bytes of system model.
	for (uint8_t i=0; i<10; i++) {
		uartSendByte(mcu_serial_number[i]);				// 10 bytes of mcu serial number
	}
	uartSendWord(logger_local_serial);					// 2 bytes of local serial.
	uartSendByte(FIRMWARE_GET_VERSION);					// 1 byte of firmware version
	uartSendWord(mcu_firmware_crc16_xmodem);			// 2 bytes of firmware CRC16-xmodem
	uartSendWord(logger_battery_level);					// 2 bytes of battery level or 0 if not available
	uartSendWord(errorGetFlags());						// 2 bytes of error flags (0x0000 for no errors)
	uartSendWord(0x0000);								// 2 bytes reserved for future use
	// Hereon depends on status. Work on this.
	uartSendByte(logger_status);						// Status: [I]dle, [L]ogging, [H]olding, [D]ownloaded.
	if (logger_status=='L') {
		uartSendWord(logger_countdown);					// 2 bytes of logger countdown (deferral)
		uartSendWord(logger_eightseconds_count);		// 2 bytes of current eightseconds count.
		uartSendWord(logger_interval);					// 2 bytes of logger interval.		
	} else if (logger_status=='H') {
		uartSendWord(0x0000);							// 2 bytes of data length
		uartSendWord(0x0000);							// 2 bytes of data CRC
		uartSendWord(0x0000);							// 2 bytes reserved for future use
	} else if (logger_status=='I') {
		uartSendWord(0x0000);							// 2 bytes reserved for future use
		uartSendWord(0x0000);							// 2 bytes reserved for future use
		uartSendWord(0x0000);							// 2 bytes reserved for future use
	} else {
		uartSendWord(0x0000);							// 2 bytes reserved for future use
		uartSendWord(0x0000);							// 2 bytes reserved for future use
		uartSendWord(0x0000);							// 2 bytes reserved for future use
	}

	_delay_ms(RX_RECEIVE_TIMEOUT);		// Wait for command string to arrive at rx_buffer_array via RX ISR.
										// Delay instead of polling or counting bytes prevents lock-down.
	uartDisable();						// End of command string receival. Shut the uart module down.
	
	if (rx_buffer_index == 31) {		// Received a command string, exactly.
		if (logger_status == 'I') {
			if (rx_buffer_array[0] == 'B') {
				uint8_t timestamp_error = 0;
				// Check that rx_buffer_array bytes 1-18 are all numbers ('0' to '9')
				for (uint8_t i=1; i<15; i++) { // Check timestamp section of command string
					if ((rx_buffer_array[i] < '0') || (rx_buffer_array[i] > '9')) {
						timestamp_error = 1;
						break;
					}
				}
				if ((rx_buffer_array[19]!='A') && (rx_buffer_array[19]!='L')) {
					timestamp_error = 1;
				}
				// Check that rx_buffer_array byte 19 is either 'A' or 'L' (Atomic or Local)
				if(!timestamp_error) {		// Timestamp seems ok, proceed to begin logging.
					logger_countdown = rx_buffer_array[15];
					logger_countdown <<= 8;
					logger_countdown |= rx_buffer_array[16];
					logger_interval = rx_buffer_array[17];
					logger_interval <<= 8;
					logger_interval |= rx_buffer_array[18];
					// Copy format version (0x01), header length (0x16), two RFU bytes (0x0000), and rx_buffer_array 1-18 to first 22 bytes in EEPROM.
					spiEnable();
					memoryWriteByte(0x0000, 0x01);				// Format version
					memoryWriteByte(0x0001, 22);				// Length of header
					memoryWriteWord(0x0002, 0x0000);			// Reserved for future use
					memoryWriteArray(0x0004, &rx_buffer_array[1], 18);	// Copy timestamp, countdown, and interval to memory array.
					spiDisable();
					logger_memory_location = 22;				// Set for next temperature log
					// Set logger_status to 'L'
					logger_status = 'L';
				
					logger_eightseconds_count = 0;				// Reset eightseconds counter.
				} else {
					;// What if timestamp has error?
				}
			} else if (rx_buffer_array[0] == 'D') {	// Only temporary, Idle logger cannot have data.
				memoryDumpAll();
				// Stream out all memory bytes, byte per byte
				// from 0 to logger_memory_location
			} else if (rx_buffer_array[0] == 'U') {
				;// Do a firmware update. Here, just reset device.
			}
			// End of logger_status == 'I'

				
		} else if (logger_status == 'L') {
			if (rx_buffer_array[0]=='E') {
				;// Do actions to end logging
			} else if (rx_buffer_array[0]=='D') {
				;// Download data while logging.
			}
		} else if (logger_status=='H') {
			if (rx_buffer_array[0]=='D') {
				;// Download data and mark as downloaded
			}
		} else if (logger_status=='D') {
			if (rx_buffer_array[0]=='C') {
				;// Clear memory and set to idle.
			}
		} else {
			; // Some other logger status?? Flag error.
		}
	} else {
		errorSetFlag(ERROR_FLAG_COMMAND_ERR);
	}
	// Check rx_buffer and copy contents to memory chip.
}
/*
void systemReadFlashLocation(void) {
	for (uint16_t k=(_system_bootloader_start-1); k>0; k--) {
		if (pgm_read_byte(k) != 0xFF) {
			_system_flash_location = k;
			break;
		}
	}

uint16_t systemCalculateAppCrc(uint16_t last_firmware_byte) {
	uint16_t firmware_crc = 0;
	for (uint16_t l=0; l<=_system_flash_location; l++) {
		firmware_crc = _crc_xmodem_update(firmware_crc, pgm_read_byte(l));
	}
	return firmware_crc;
}
*/

// These are a couple of examples of code that at some time worked.

/*



		memoryUltraDeepPowerDownExit();	
		memoryReadMFDID();
		memoryPrintMFDID();
		memoryUltraDeepPowerDownEnter();
*/
		
