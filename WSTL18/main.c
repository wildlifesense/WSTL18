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
#include "uart.h"
#include "wstl18.h"

uint8_t  uart_listening;				// This could move to the UART module.


int main(void) {
	DDRE |= (1<<DDRE2);
	uart_listening = 0;
	uartEnable();
    while (1) 
    {
		PORTE |= (1<<PORTE2);
		_delay_ms(5);
		PORTE &= ~(1<<PORTE2);
		_delay_ms(90);
		PORTE |= (1<<PORTE2);
		_delay_ms(5);
		PORTE &= ~(1<<PORTE2);
		uartSendByte('O');
		_delay_ms(1900);
    }
}

