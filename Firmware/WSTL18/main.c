/********************************************************************************

  main.c: The main routine of the WSTL18 firmware.
 
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
#include <avr/sleep.h>
#include "uart.h"
#include "wstl18.h"
#include "led.h"		// Remove
#include "rtc.h"		// Remove


int main(void) {
	wstl18Init();

	


	


    while (1) {
		sleep_mode();
		wstl18DoubleBlink();
		// Check status of PC3
		/*
		if (!(PINC & (1<<PINC3))) {
			uartEnable();
			_delay_ms(100);		// uart wouldn't work without this delay.
			uartSendByte('0');
			uartDisable();
		}
		*/
    }
}