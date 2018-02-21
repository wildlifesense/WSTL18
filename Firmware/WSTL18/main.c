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
#include <util/delay.h>
#include "uart.h"
#include "spi.h"
#include "system.h"
#include "memory.h"
#include "max30205.h"
#include "indicator.h"
#include "host.h"

char     logger_status;
uint16_t logger_countdown;
uint16_t interval_limit;				// How many eight seconds counts till logging interval reached.
volatile uint32_t total_eightseconds_counter;			// Count of eight seconds intervals since start of timer.

#define _SYSTEM_RX_BUFFER_LENGTH 128
volatile uint8_t _system_rx_buffer_index;								// Points to current writing location in buffer.
volatile uint8_t _system_rx_buffer_array[_SYSTEM_RX_BUFFER_LENGTH];		// Buffer for bytes received through uart.

/*
 * Timer 2 is the Real Time Counter, set to fire every 8 seconds.
 */
ISR(TIMER2_OVF_vect) {
	total_eightseconds_counter++;
}


#define HOST_COMMAND_BUFFER_LENGTH	128
#define HOST_COMMAND_RECEIVE_TIMEOUT	200					// Time window to receive command, in ms

volatile uint8_t _host_command_index;								// Points to current location in buffer.
volatile char _host_command_buffer[HOST_COMMAND_BUFFER_LENGTH];	// Received command.

ISR(USART0_RX_vect) {
	hostCommandInterruptHandler();
}
/*******************************/
void hostCommandInterruptHandler() {
	_host_command_buffer[_host_command_index] = UDR0;
	_host_command_index++;
}
/*******************************/


int main(void) {
	systemInit();
	max30205Init();
	memoryInitialize();
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
	hostCommandPrint();						// Send command back for verification.
	uartDisable();
}