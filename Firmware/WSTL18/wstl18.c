/********************************************************************************

  wstl18.c: Initialization and general routines for the WSTL18 firmware.
 
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
#include "twi.h"
#include "spi.h"
#include "memory.h"
#include "max30205.h"
#include "uart.h"
#include "rtc.h"
#include "led.h"
#include "wstl18.h"

#define WSTL18_COMMAND_LENGTH	16
char wstl18_command_buffer[WSTL18_COMMAND_LENGTH];	// Stores currently received command and setup.
uint8_t	wstl18_command_index;						// Points to current location in buffer.

uint8_t wstl18_flags;					// Flags to indicate status of device. POR defaults to 0x00.
#define WSTL18_FLAG_LOGGING		0		// Is the device currently logging? 0: No, 1: Yes
#define WSTL18_FLAG_ERROR_MEM	1		// Encountered memory error?		0: No, 1: Yes
#define WSTL18_FLAG_ERROR_TMP	2		// Encountered error with temperature sensor?	0: No, 1: Yes
#define WSTL18_FLAG_COMMAND_OVF	3		// Command received via uart overflowed buffer.
#define WSTL18_FLAG_COMMAND_ERR	4		// General command error.
#define WSTL18_FLAG_STH1		5		// Some flag
#define WSTL18_FLAG_STH2		6

#define WSTL18_HOST_RESPOND_TIMEOUT_MS	5000	// Time host has to respond to "listening" code over uart.
												// Change to ~50ms when host is a program.

/*
 * Set a flag on this module's flags variable. Takes one of WSTL18_FLAG_... #defines.
 */
void _wstl18SetFlag(uint8_t flag_to_set) {
	wstl18_flags |= (1<<flag_to_set);
}

void _wstl18ClearFlag(uint8_t flag_to_clear) {
	wstl18_flags &= ~(1<<flag_to_clear);
}


void wstl18Init(void) {
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

	ledInit();
	rtcStart();					// Start the Real Time Counter. Takes 1000ms+ to allow crystal to stabilize.
	_delay_ms(1000);
}


void wstl18CommandClear(void) {
// 	char wstl18_command_buffer[16];			// Stores currently received command and setup.
// 	uint8_t	wstl18_command_index;			// Points to current location in buffer.
	wstl18_command_index = 0;
}

/*
 * wstl18CommandAppend: Add a (uart received) character to the command buffer unless it's overflowed.
 * If the command buffer is about to overflow, don't add the character. Instead, check the command
 * overflow bit in the system's flag byte.
 */
void wstl18CommandAppend(char appended) {
	if(wstl18_command_index < WSTL18_COMMAND_LENGTH) {
		wstl18_command_buffer[wstl18_command_index] = appended;
		wstl18_command_index++;
	} else {
		wstl18_flags |= (1<<WSTL18_FLAG_COMMAND_OVF);
	}
}

/*
 * wstl18CommandBeginLogging: Start a logging session based on current data.
 */
void wstl18CommandBeginLogging() {
	uint8_t timestamp_ok = 1;
	for (uint8_t i=1; i<=14; i++) {
		if ( (wstl18_command_buffer[i] < 0) || (wstl18_command_buffer[i] > 9) ) {
			timestamp_ok = 0;
			return;
		}
	}
	if (timestamp_ok) {
		;	// Do begin logging.
	}
	// wstl18_command_buffer[1] == 2
}

/*
 * Ok this is a big one. This function is supposed to run in just under 6 seconds, or it might get
 * affected by the next interrupt and might delay the next temperature log.
 *
 * Checks the first character of the command buffer.
 * D: Dump data,
 * B: Begin logging
 * E: End logging.	
 */
void wstl18CommandRespond(void) {
	if (wstl18_command_index > 0) {			// Received at least one command character.
		if (wstl18_command_index < WSTL18_COMMAND_LENGTH) {
			switch(wstl18_command_buffer[0]) {
				case 'D':
					;		// Dump data over uart.
					break;
				case 'B':
					wstl18CommandBeginLogging();		// Begin logging. Requires timestamp of start in YYYYMMDDHHmmSS format or is ignored.
					break;
				case 'E':
					;		// End logging. Requires timestamp of end in YYYYMMDDHHmmSS format or is ignored.
			}
		} else {			// Command buffer was overflown
			;				// TODO: Send string: Command discarded due to buffer overflow.
		}
	}
}
		// A command overflow should not occur, so buffer contents are probably garbage
		// and should be disposed of.


void wstl18UartExchange(void) {
	wstl18CommandClear();
	uartEnable();
	uartSendByte('X');
	_delay_ms(WSTL18_HOST_RESPOND_TIMEOUT_MS);				// Host has this much time to respond.
	uartDisable();	// Move this to after check?
	wstl18CommandRespond();
}


void wstl18DumpAllMemory(void) {
	// Check if memory is sleeping/busy.
	// Wake memory up.
	// Get current memory location (currently logging location or locate while reading memory.)
		// Read memory from index i.
		// Send byte over uart.
	// 
}

/*
	wstl18DoLog: Gets the temperature from the temperature logger in a non-optimized way.
	Using a timer to wait for the temperature conversion will introduce some uncertainty in control.
	It might just be best, at least for now, to utilize these 50ms to do something else, such as
	wake up the memory module or do some checks in preparation. If this works well and the timing is
	ok, I might just as well keep it like this.
*/
uint16_t wstl18DoLog(void) {
	max30205StartOneShot();
	_delay_ms(50);	//Max 50ms. Try to do other stuff here and reduce delay accordingly.
	return max30205ReadTemperature();
}

void wstl18Blink(void) {
	PORTE |= (1<<PORTE2);
	_delay_ms(1);
	PORTE &= ~(1<<PORTE2);
}