/********************************************************************************

  rtc.c: Real time counter module of WSTL18 firmware.
 
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
#include <avr/interrupt.h>
#include <util/delay.h>
#include "rtc.h"

uint16_t interval_limit;				// How many eight seconds counts till logging interval reached.
uint32_t eightseconds_counter;			// Count of eight seconds intervals since start of timer.


/*
 * Timer 2 is the Real Time Counter.
 */
ISR(TIMER2_OVF_vect) {
	rtcIntervalCounterIncrement();
}
/*
// Initialize TC2 for Real Time Clock operation from external 32.768kHz crystal.
void rtcInit(void) {
	PRR0	&= ~(1<<PRTIM2);
	ASSR	|= (1<<AS2);											// Clock from external crystal.
	TCCR2B	|= (1<<CS20)|(1<<CS21)|(1<<CS22);						// Tosc/1024 prescaler = 8sec to overflow.
	TIMSK2	= 0;
	TIMSK2	|= (1<<TOIE2);											// Enable overflow interrupt
	TCNT2	= 0;													// Clear counter value.
	sei();
}
*/
/*
 * rtcInit: Initialize TC2 for Real Time Clock operation from external 32.768kHz crystal.
 * The crystal should be allowed around 1000ms for stabilization before it is actually used.
 * Considering this is only done upon battery insertion or start of a new log, it should be
 * safe to just wait for this time before accepting the starting timestamp.
 */
void rtcStart(void) {
	PRR0	&= ~(1<<PRTIM2);
	ASSR	|= (1<<AS2);											// Clock from external crystal.
	TCCR2B	|= (1<<CS20)|(1<<CS21)|(1<<CS22);						// Tosc/1024 prescaler = 8sec to overflow.
	TIMSK2	= 0;
	TIMSK2	|= (1<<TOIE2);											// Enable overflow interrupt
	//_delay_ms(1000);												// Allow RTC crystal to stabilize (RTC AN p.5).
	TCNT2	= 0;													// Clear counter value.
	sei();
	interval_limit = 75;											// Default to 10 minutes.
	rtcIntervalCounterClear();
}

/*
 * Won't the unit be disabled if I disable the RTC? Who's going to wake up the device?
 */
void rtcStop(void) {
	TCCR2B &= ~( (1<<CS20)|(1<<CS21)|(1<<CS22) );					// Set CS20, CS21, CS22 bits to 0 to stop TC2 clock.
	PRR0 |= (1<<PRTIM2);											// Cut off clock to TC2.
	rtcIntervalCounterClear();
}


void rtcIntervalCounterIncrement(void) {
	eightseconds_counter++;
}

uint32_t rtcIntervalCounterGet(void) {
	return eightseconds_counter;
}

void rtcIntervalCounterClear(void) {
	eightseconds_counter = 0;
}