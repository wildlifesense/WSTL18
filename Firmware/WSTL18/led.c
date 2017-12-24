/********************************************************************************

  led.c: Light emitting diode indicator module of WSTL18 firmware.
 
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
#include <util/delay.h>						// F_CPU has been defined in AVR/GNU C compiler -> Symbols -> Defined symbols (-D)
#include "led.h"

/*
 * Initialize the LED. Keep in mind this can suffer from leakage current.
 */
void ledInit(void) {
	(LED_DDR |= (1<<LED_DDR_BIT));					// Set LED pin as output
	(LED_PORT &= ~(1<<LED_PORT_BIT));				// Set it to LOW.
}

void ledOn(void) {
	(LED_PORT |= (1<<LED_PORT_BIT));
}


void ledOff(void) {
	LED_PORT &= ~(1<<LED_PORT_BIT);
}


/*
 * Flash LED to indicate an OK condition.
 * Later implement this with a timer, but it's ok for now.
 */
void ledFlashOk(void) {
	ledOn();
	_delay_ms(10);
	ledOff();
}

// Do a double-flash to indicate some sort of error.
// Later implement this with timers, but it's ok for now.
void ledFlashError(void) {
	ledFlashOk();
	_delay_ms(200);
	ledFlashOk();
}

void ledBlink(uint8_t num_blinks, uint16_t light_duration, uint16_t dark_duration) {
	// Start a timer to do the above
	// Don't forket to stop timer in PRR at the end of blink.
}
