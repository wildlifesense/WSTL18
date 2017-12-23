/********************************************************************************

  led.h: Header file for the light emitting diode indicator module of WSTL18 firmware.
 
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
#ifndef LED_H_
#define LED_H_

/* User can modify these if LED DDR/PROT are altered.
 * WSTL18: LED is PE2
 */
#define LED_DDR			DDRE
#define LED_DDR_BIT		DDRE2
#define LED_PORT		PORTE
#define LED_PORT_BIT	PORTE2

void ledInit(void);								// Initialize pins for status LEDs
void ledOn(void);								// Switch the LED on.
void ledFlashOk(void);
void ledFlashError(void);

#endif /* LED_H_ */