/********************************************************************************

  spi.c: SPI module of WSTL18 firmware.
 
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
#include "spi.h"

// #################### PORTS & PINS FOR SPI0. Don't change unless porting to another mcu ####################
//	MISO0 is PB4
//	SCK0 is PB5
//	MOSI0 is PB3
//	SS0 is PB2
// What about slave configuration?
#define SPI0_MISO_MASTER_INIT		DDRB &= ~(1<<DDRB4);								// MISO stays as input
#define SPI0_SCK_MASTER_INIT		DDRB |=  (1<<DDRB5);									// SCK is output
#define SPI0_MOSI_MASTER_INIT		SPI0_MOSI_DDR |=  (1<<SPI0_MOSI_DDR_BIT);								// MOSI is output
#define SPI0_SS_MASTER_INIT	SPI0_SS_DDR |= (1<<SPI0_SS_DDR_BIT);SPI0_SS_PORT |= SPI0_SS_PORT_BIT	// OUTPUT and HIGH
#define SPI0_SS_SLAVE_INIT	SPI0_SS_DDR &= ~(1<<SPI0_SS_DDR_BIT);SPI0_SS_PORT |= SPI0_SS_PORT_BIT	//

#define SPI0_MISO_RELEASE			DDRB &= ~(1<<DDRB4);PORTB|=(1<<PORTB4)			// Input with pull-up
#define SPI0_SCK_RELEASE			DDRB &= ~(1<<DDRB5);PORTB|=(1<<PORTB5)			// Input with pull-up
#define	SPI0_MOSI_RELEASE			DDRB &= ~(1<<DDRB3);PORTB|=(1<<PORTB3)			// Input with pull-up
#define SPI0_SS_RELEASE				DDRB &= ~(1<<DDRB2);PORTB|=(1<<PORTB2)			// Input with pull-up


#define SPI_PRESCALER_4		0b00000000
#define SPI_PRESCALER_16	0b00000001
#define SPI_PRESCALER_64	0b00000010
#define SPI_PRESCALER_128	0b00000011
#define SPI_PRESCALER_MASK	0b00000011

#define SPI_MODE_0			0b00
#define SPI_MODE_1			0b01
#define SPI_MODE_2			0b10
#define SPI_MODE_3			0b11
#define SPI_MODE_MASK		0b00001100

// #################### INTERNAL FUNCTIONS #################### //

void spiSetMode(uint8_t mode) {
	if (0<=mode && mode<=3) {
		SPCR0 &= ~(SPI_MODE_MASK);
		SPCR0 |= (SPI_MODE_MASK & (mode<<2));
	}
}
void spiSetMaster(void) {
	SPCR0 |= (1<<MSTR);				// Set to master mode. What about SCK, MISO, MOSI, and SS?
}
void spiSetSlave(void) {
	SPCR0 &= ~(1<<MSTR);			// Set to slave mode. What about SCK, MISO, MOSI, and SS?
}
void spiSetMSB(void) {
	SPCR0 |= (1<<DORD);
}
void spiSetLSB(void) {
	SPCR0 &= ~(1<<DORD);
}

// Enable SPI0 module
void spiEnable(void) {
	SPCR0 |= (1<<SPE);
}
// Disable SPI0 module
void spiDisable(void) {
	SPCR0 &= (1<<SPE);
}

// Set the SPI0 prescaler.
void spiSetPrescaler(uint8_t prescaler) {
	SPCR0 &= ~(SPI_PRESCALER_MASK);						// Clear prescaler bits
	prescaler &= SPI_PRESCALER_MASK;					// Filter prescaler parameter
	SPCR0 |= prescaler;			// Set prescaler
}

// Set SPI0 double clock rate. Fspi0 calculated by half prescaler.
void spiSetDoubleSpeed(void) {
	SPSR0 |= (1<<SPI2X);            // Double Clock Rate
}

// Disable SPI0 double clock rate. Fspi0 calculated by prescaler.
void spiSetSingleSpeed(void) {
	SPSR0 &= ~(1<<SPI2X);			// Not double clock rate
}

// Enable SPI0 interrupts
void spiInterruptEnable(void) {
	SPCR0 |= (1<<SPIE);
}

// Disable SPI0 interrupts
void spiInterruptDisable(void) {
	SPCR0 &= ~(1<<SPIE);
}

void spiPowerUp(void) {
	PRR0 &= ~(1<<PRSPI0);										// Clock SPI0 clock, powers the module
	
}

void spiPowerDown(void) {
	PRR0 |= (1<<PRSPI0);										// Cut off SPI0 clock, shuts the module.
}
// ###############  END INTERNAL FUNCTIONS #################### //


// Initialize pins for spi communication
void spiInit(void) {
	spiPowerUp();								// Make sure SPI0 clock isn't stopped for power-save.
	spiInterruptDisable();						// SPI0 Interrupt. Turn this into a function.
	spiSetPrescaler(SPI_PRESCALER_64);			// Prescaler at 64 for testing phase.
	spiSetSingleSpeed();						// Same as doing nothing
	spiSetMode(SPI_MODE_0);					// 
	spiSetMaster();
	spiEnable();								//
}

/*
 * spiStop: Stop the SPI module and set it for lowest power consumption.
 */
void spiStop(void) {											// TODO: Should this be "terminate" or something?
	spiInterruptDisable();										// Disable SPI0 interrupt
	spiDisable();
	spiPowerDown();												// Cut off SPI0 clock, shuts the module	
	DDRB  &= ~(1<<DDRB2|1<<DDRB3|1<<DDRB4|1<<DDRB5);			// SS0, MOSI0, SCK0, MISO0. All pins are input
	PORTB |=  (1<<PORTB2|1<<PORTB3|1<<PORTB4|1<<PORTB5);		// Pull-up enabled, reduces consumption.
}


uint8_t spiTradeByte(uint8_t byte) {
	SPDR0 = byte;										/* SPI starts sending immediately */
	loop_until_bit_is_set(SPSR0, SPIF);					/* wait until done */
	return SPDR0;
}

// Shift an array out and retain returned data.
void spiExchangeArray(uint8_t * dataout, uint8_t * datain, uint8_t len) {
	for (uint8_t i = 0; i < len; i++) {
		datain[i] = spiTradeByte(dataout[i]);
	}
}

// Shift an array out and discard returned data.
void spiTransmitArray(uint8_t * dataout, uint8_t len) {
	for (uint8_t i = 0; i < len; i++) {
		spiTradeByte(dataout[i]);
	}
}