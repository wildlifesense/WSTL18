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
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include "uart.h"
#include "wstl18.h"
#include "led.h"		// Remove
#include "rtc.h"		// Remove
#include "memory.h"
#include "spi.h"
#include "twi.h"
#include "max30205.h"

#include <ctype.h>

int main(void) {
	wstl18Init();
	
	uint16_t max30205_temp = 32;			// 16-bit temperature from sensor.
	double lsb_celcius = 0.00390625;		// How many celcius is one lsb from sensor?
	char max30205_tempstr[6];				// Char array for temperature in celcius.
	
	char string1[20] = "Character is: ";
	uartEnable();
	_delay_ms(100);
	char a = 'a';
	//set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    while (1) {
		for(uint8_t i=0; string1[i]; i++) {
			uartSendByte(string1[i]);
		}
		uartSendByte(a);
		uartSendByte('\n');
		_delay_ms(0);
		a++;
		if(!isalpha(a)) {
			a = 'a';
		}
/*		
		sleep_mode();
		wstl18DoubleBlink();
		twiEnable();
		_delay_ms(100);
		max30205_temp = max30205ReadTemperature();
		twiDisable();


		double whole = (double)max30205_temp * lsb_celcius;
		whole = round(whole*10) / 10;
		sprintf(max30205_tempstr, "%.1f", (double)whole);

		uartEnable();
		_delay_ms(100);

		uartSendByte((uint8_t) (max30205_temp >> 8));
		uartSendByte((uint8_t) max30205_temp);

		//uartSendString(max30205_tempstr);
		uartDisable();
*/
   }
}