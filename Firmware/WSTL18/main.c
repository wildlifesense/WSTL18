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
#include "memory.h"
#include "spi.h"
#include "max30205.h"

int main(void) {
	wstl18Init();
	max30205Init();
	memoryInitialize();

	spiEnable();
	memoryOTPLoad();
	spiDisable();
	memoryOTPPrint();
	
	set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    while (1) {
		sleep_mode();
		wstl18Blink();
/*
		max30205StartOneShot();
		_delay_ms(50);
		temperature = max30205ReadTemperature();
		temp_MSB = (uint8_t) (temperature>>8);
		temp_LSB = (uint8_t) temperature;
*/		
		// Send data over uart
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
	   }
}