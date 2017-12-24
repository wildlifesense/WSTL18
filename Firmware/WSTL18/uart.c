/********************************************************************************

  uart.c: Uart interface module of WSTL18 firmware.
 
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
#include "uart.h"

/*
 * A byte was received on the RX UART. Remember to enable UCSR0B.RXCIE0
 * This ISR changes per project, so it should be in project-specific source file, not in library.

ISR(USART0_RX_vect) {
	//uartIsrHandler(UDR0);
	//RED_BLINK;				// This prevents TX data from being mangled. Why?
}
 */


void _uartSetBaud1M(void) {
	// I'm hard-setting baud rate to 1M.
	// See table in DS p.249 for baud rate examples
	UBRR0 =  0;												// 103 for 9600BAUD, 0 for 1MBAUD (at 8MHz)
	UCSR0A |= (1<<U2X0);
}
void _uartSetBaud500k(void) {
	UBRR0 = 1;
	UCSR0A |= (1<<U2X0);
}
void _uartSetBaud250k(void) {
	UBRR0 = 3;
	UCSR0A |= (1<<U2X0);
}

void uartEnable(void) {
	PRR0 &= ~(1<<PRUSART0);
	_uartSetBaud1M();
	UCSR0B |= (1 << RXEN0)|(1 << TXEN0);//|(1<<RXCIE0);		// Enable USART transmitter/receiver.
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);					// 8 data bits (plus 2 stop bits).
	//UCSR0D |= (1<<SFDE);									// Enable wakeup on RX interrupt (DS p.259)
	//sei();												// Not using interrupts in WSTL18
}

/*
 * Enable RX complete interrupt (UART RX must be enabled first).
 */
void uartEnableRxInterrupt(void) {
	UCSR0B |= (1<<RXCIE0);
}

/*
 * Disable RX complete interrupt (UART RX must be enabled).
 */
void uartDisableRxInterrupt(void) {
	UCSR0B &= ~(1<<RXCIE0);
}

/*
 * uartDisable: Stop the UART module so it's at low-power mode.
 */
void uartDisable(void) {
	PRR0 |= (1<<PRUSART0);
	UCSR0B &= ~((1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0));			// Disable UART transmitter, receiver, and RX complete interrupt.
	DDRD &= ~((1<<DDRD0)|(1<<DDRD1));						// Set pins PD0 and PD1 to input with pull-up.
	PORTD |= ((1<<PORTD0)|(1<<PORTD1));						// RX0 is PD0, TX0 is PD1
}

// Send a byte over usart.
void uartSendByte(uint8_t data) {
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = data;
}


/*
 * Send a string of up to 255 characters over USART
 */
void uartSendString(const char myString[]) {
    for (uint8_t i=0; myString[i]; i++)
        uartSendByte(myString[i]);
}


/*
 *	Send a two-byte integer as characters to the uart bus.
 */
void uartPrintWord(uint16_t word) {
	uartSendByte('0' + (word / 10000));                 /* Ten-thousands */
	uartSendByte('0' + ((word / 1000) % 10));               /* Thousands */
	uartSendByte('0' + ((word / 100) % 10));                 /* Hundreds */
	uartSendByte('0' + ((word / 10) % 10));                      /* Tens */
	uartSendByte('0' + (word % 10));                             /* Ones */
}

void uartPrintBinaryByte(uint8_t byte) {
	/* Prints out a byte as a series of 1's and 0's */
	uint8_t bit;
	for (bit = 7; bit < 255; bit--) {
		if (bit_is_set(byte, bit))
		uartSendByte('1');
		else
		uartSendByte('0');
	}
}


void uartPrintBinaryWord(uint16_t word) {
	// Prints out a word as a series of 1's and 0's
	uartPrintBinaryByte((uint8_t)(word>>8));
	uartPrintBinaryByte((uint8_t)word);
}