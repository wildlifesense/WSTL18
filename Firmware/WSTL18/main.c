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
#include "rtc.h"
#include "error.h"

#include "uart.h"
#include "spi.h"
#include "system.h"
#include "memory.h"
#include "max30205.h"
#include "indicator.h"
#include "host.h"

#define system_soft_reset()        \
do                          \
{                           \
	wdt_enable(WDTO_15MS);  \
	for(;;)                 \
	{                       \
	}                       \
} while(0)


char     logger_status;
uint16_t logger_countdown;
uint16_t interval_limit;				// How many eight seconds counts till logging interval reached.
volatile uint32_t total_eightseconds_counter;			// Count of eight seconds intervals since start of timer.

#define SYSTEM_FIRMWARE_MAJOR 0		// Range 0-9 then A-F
#define SYSTEM_FIRMWARE_MINOR 1		// Range 0-9 then A-F

#define _SYSTEM_RX_BUFFER_LENGTH 128
volatile uint8_t _system_rx_buffer_index;								// Points to current writing location in buffer.
volatile uint8_t _system_rx_buffer_array[_SYSTEM_RX_BUFFER_LENGTH];		// Buffer for bytes received through uart.

uint8_t system_id[3];					// Identifies a 328PB.
uint8_t system_serial_number[10];				// Unique serial number.
// Firmware and bootloader parameters.
uint16_t _system_firmware_crc;
uint16_t _system_firmware_last_byte;	// Byte address of last byte of current firmware. Helps calculate firmware size and CRC.
uint16_t _system_bootloader_start;		// Byte address of first bootloader byte. Depends on BOOTSZ fuses.
uint16_t _system_available_pages;		// Number of pages available for application firmware.


/*
 * Timer 2 is the Real Time Counter, set to fire every 8 seconds.
 */
ISR(TIMER2_OVF_vect) {
	total_eightseconds_counter++;
}


#define HOST_COMMAND_BUFFER_LENGTH	128
#define HOST_COMMAND_RECEIVE_TIMEOUT	200					// Time window to receive command, in ms

volatile uint8_t _host_command_index;								// Points to current location in buffer.
volatile char _host_command_buffer[HOST_COMMAND_BUFFER_LENGTH];		// Received command.

ISR(USART0_RX_vect) {
	hostCommandInterruptHandler();
}
/*******************************/
void hostCommandInterruptHandler() {
	_host_command_buffer[_host_command_index] = UDR0;
	_host_command_index++;
}
/*******************************/

uint8_t systemGetVersion(void) {
	return (uint8_t) (SYSTEM_FIRMWARE_MAJOR<<4|SYSTEM_FIRMWARE_MINOR);
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
	rtcStart();					// Start the Real Time Counter. Takes 1000ms+ to allow crystal to stabilize.

	
	// Initialize temperature sensor chip into lowest power consumption.
	max30205Init();
	
	// Initialize memory chip into lowest power consumption.
	memoryInitialize();

	// Load MCU ID.
	system_id[0] = boot_signature_byte_get(0);
	system_id[1] = boot_signature_byte_get(2);
	system_id[2] = boot_signature_byte_get(4);
	
	// Load MCU serial number.
	for (uint8_t i=0; i<10; i++) {
		system_serial_number[i] = boot_signature_byte_get(i+14);	// Read signature bytes 14-23
	}

	// Load bootloader start address and firmware available pages
	uint8_t bootsz_bits = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
	bootsz_bits = bootsz_bits & 0b00000110;					// Keep only bootsz bits. 1: unseet, 0: set
	bootsz_bits = bootsz_bits >> 1;									// Move bootsz bits to base
	bootsz_bits = ~(bootsz_bits) & 0b11;							// Reverse bootsz bits and filter out other bits
	// Bootsz bits are now 0-3 representing 4, 8, 16, or 32 pages of boot sector.
	_system_bootloader_start = 0x8000 - (1<<bootsz_bits)*512;		// bootsector size = math.pow(2, bootsz) * 512
	_system_available_pages = _system_bootloader_start / SPM_PAGESIZE;

	// Find the last byte of firmware in flash.
	for (uint16_t k=(_system_bootloader_start-1); k>0; k--) {
		if (pgm_read_byte(k) != 0xFF) {
			_system_firmware_last_byte = k;
			break;
		}
	}

	// Calculate firmware xmodem CRC16
	_system_firmware_crc = 0;
	for (uint16_t i=0; i<=_system_firmware_last_byte; i++) {
		_system_firmware_crc = _crc_xmodem_update(_system_firmware_crc, pgm_read_byte(i));
	}


	// Set the initial logger status.
	logger_status = 'I';
	
	set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    while (1) {
		sleep_mode();
		switch (logger_status) {
			case 'L':
				if (logger_countdown>0) {
					logger_countdown--;
				} else if (eightseconds<interval) {
					eightseconds += 1
				}
				do_log();
				break;
			case 'H'
		}
		if ( !(PINC & (1<<PINC3))) { // PD2 is down. NOTE: PC3 is temporary for NestProbe TL1P1. Use PD2 for NestProbe TL1P2/P3
			hostCommandReceive();
		}
		indicatorShortBlink();
	}
}



/*
		max30205StartOneShot();
		_delay_ms(50);
		temperature = max30205ReadTemperature();
		temp_MSB = (uint8_t) (temperature>>8);
		temp_LSB = (uint8_t) temperature;
*/		
		// Send data over uart
		/*
		uartEnable();
		uartSendByte('-');
		uartSendByte(1);
		uartSendByte(2);
		uartSendByte('+');
		uartDisable();
	
		memoryUltraDeepPowerDownExit();
		
		memoryReadMFDID();
		memoryPrintMFDID();

		memoryUltraDeepPowerDownEnter();
		*/
		

/*
 * Clear the rx buffer and its index.
 */
void systemRxBufferClear(void) {
	for(uint8_t i=0; i<_SYSTEM_RX_BUFFER_LENGTH; i++) {
		_system_rx_buffer_array[i] = 0;
	}
	_system_rx_buffer_index = 0;
}


void hostCommandReceive(void) {
	hostCommandClear();
	uartEnable();
	uartRxInterruptEnable();
	uartSendString("TL1");
	for (uint8_t i=0; i<3; i++) {	// 3 ID bytes
		uartSendByte(systemGetId(i));
	}
	for (uint8_t i=0; i<10; i++) {	// 10 serial bytes
		uartSendByte(systemGetSerial(i));
	}
	/*
		Also send one byte for current status:
			I is idle,
			D is deferred counting,
			L is logging,
			R means resident data were found in non-volatile memory on startup. TL1 will not accept logging instructions until residual data are downloaded and cleared.
			U is unknown/undefined status. Firmware should check before resetting.
	*/
	uartSendByte('I');
	_delay_ms(HOST_COMMAND_RECEIVE_TIMEOUT);	// Delay instead of polling prevents lock-down.
	uartDisable();
}