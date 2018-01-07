/********************************************************************************

  twi.c: Two-wire interface (aka I2C) module of WSTL18 firmware.
 
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
#include <util/twi.h>								// Useful TWI bit mask definitions
#include "twi.h"


// DS p. 280 and http://www.atmel.com/webdoc/AVRLibcReferenceManual/ch20s33s01.html

// #################### INTERNAL USE FUNCTIONS ####################

static void twiSetPrescaler(uint8_t prescaler) {
	TWSR0 &= ~(TWI_PRESCALER_MASK);		// Clear prescaler bits
	TWSR0 |= (prescaler);				// Set prescaler
}

// Wait for TWI module to complete current action.
static void twiWaitForComplete(void) {
	loop_until_bit_is_set(TWCR0, TWINT);
}

// Send start condition on TWI line.
static void twiSendStartCondition(void) {
	TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
	twiWaitForComplete();
}
// Set TWINT to begin transmission
static void twiTransmitNoAck(void) {
	TWCR0 = (1<<TWINT) | (1<<TWEN);
	twiWaitForComplete();
}

static void twiTransmitAck(void) {
	TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
	twiWaitForComplete();
}
/*
 *	twiSendStartConditionCheck: Use this to check TWI configuration and lines.
 *	It doesn't actually send any bytes, but if you're having trouble working
 *	the TWI you can check the return value. A 1 indicates problems in hardware
 *	or setup, a 0 indicates all is good so far.
 *	(When I made this function it ended up being PRR0.PRTWI0 was up.)
 */
uint8_t twiSendStartConditionCheck(void) {
    uint8_t twst;
	twiSendStartCondition();
    twst = TWSR0 & 0XF8;												// Mask prescaler bits
    if( (twst != TW_START) && (twst != TW_REP_START)) return 1;			// Check value of TWI status register
	return 0;
}
// #################### END OF INTERNAL USE FUNCTIONS ####################





// #################### PUBLIC LOW LEVEL FUNCTIONS ####################

// Set line speed and activate TWI0
void twiEnable(void) {
	PRR0  &= ~(1<<PRTWI0);					// Start clock to TWI peripheral.
	twiSetPrescaler(TWI_PRESCALER_1);
	TWBR0 = 2;								// 32 for 100k, 2 for 400k (with prescaler at 1).
	TWCR0 |= (1<<TWEN);						// Enable TWI
}

// Deactivate the TWI0 module and set SDA and SCL pins to low power consumption.
void twiDisable(void) {
	TWCR0 &= ~(1<<TWEN);					// Disable TWI, may not be necessary
	PRR0  |= (1<<PRTWI0);					// Stop TWI clock
	DDRC  &= ~((1<<DDRC4)|(1<<DDRC5));		// Set TWI pins as inputs
	PORTC |= ((1<<PORTC4)|(1<<PORTC5));		// Pull them up to reduce power consumption.
}



// Issue a stop condition on the TWI line
void twiStop(void) {
	TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	loop_until_bit_is_clear(TWCR0, TWINT);			// Wait until stop condition is executed and bus released.

}

void twiSend(uint8_t data) {
	TWDR0 = data;
	twiTransmitNoAck();
}
// Read in from slave, sending ACK when done (sets TWEA).
uint8_t twiReadAck(void) {
	twiTransmitAck();
	return(TWDR0);
}
// Read in from slave (or master) without sending ack
uint8_t twiReadNoAck(void) {
	twiTransmitNoAck();
	return(TWDR0);
}

// #################### END PUBLIC LOW LEVEL FUNCTIONS ####################


// ######################## HIGH LEVEL FUNCTIONS ########################### 
/*
 *  Issues a start condition and sends address with transfer direction (R/W).
 *  Return 0 = device accessible, 1 = failed to access device
 */
uint8_t twiStart(uint8_t address) {
    uint8_t twst;
    // Send start condition
    twiSendStartCondition();
    // Check value of TWI status register. Mask prescaler bits
    twst = TWSR0 & 0XF8;
    if( (twst != TW_START) && (twst != TW_REP_START)) {
		return 1;
	}
    // Send device address
    twiSend(address);						// Transmit address and R/W bit
    // Check value of TWI status register. Mask prescaler bits
    twst = TWSR0 & 0XF8;
    if ( (twst != TW_MT_SLA_ACK) && (twst != TW_MR_SLA_ACK) )
		return 1;
    return 0;
}


/*
 *  Issues a start condition and sends address and transfer direction.
 *  If device is busy, use ack polling to wait until device is ready.
 */
void twiStartWait(uint8_t address) {
    uint8_t twst;

    while(1) {
        // Send start condition
		twiSendStartCondition();
        // Check value of TWI status register. Mask prescaler bits
        twst = TWSR0 & 0XF8;
        if( (twst != TW_START) && (twst != TW_REP_START)) continue;

        // Send device address
		twiSend(address);

        // Check value of TWI status register. Mask prescaler bits
        twst = TWSR0 & 0XF8;
        if ( (twst == TW_MT_SLA_NACK) || (twst == TW_MR_DATA_NACK) ) {
            twiStop();													// Device busy, stop operation.
            
            continue;
        }
        // if( twst != TW_MT_SLA_ACK) return 1
        break;
    }
}

/*
 *	twiStream: Sends a stream of bytes to a slave device over TWI bus.
 *
 *	Input:
 *		address: The 8-bit address of the slave device.
 *		*data:	A null-terminated array of 8-bit values (string)
 *
 *	This function is mainly used to send data to a character LCD display connected via TWI.
 */
void twiStream(uint8_t address, uint8_t *data) { // Expects a '\0' terminated string
	twiStart(TWI_WRITE(address));
	for(uint8_t i=0; data[i]; i++) {
		twiSend(data[i]);
	}
	twiStop();
}


/*
 *	twiReadRegister8: Reads data from an 8-bit register of slave device.
 *
 *	Requires a working TWI interface.
 *
 *	Input:
 *		slave_address:		The 8-bit slave address of connected TWI device. Address is on 7 MSBs.
 *		register_address:	The 8-bit address to be read from slave.
 *
 *	Output:
 *		8-bit value of the register.
 */
uint8_t twiReadRegister8(uint8_t slave_address, uint8_t register_address) {
	uint8_t register_data;
	twiStart(TWI_WRITE(slave_address));
	twiSend(register_address);
	twiStart(TWI_READ(slave_address));
	register_data = twiReadNoAck();
	twiStop();
	return register_data;
}
/*
 *	twiReadRegister16: Reads data from a 16-bit register of slave device.
 *
 *	Requires a working TWI interface.
 *	
 *	Input:
 *		register_address: An 8-bit register address. Best to select a defined address macro from the header file.
 *	Output:
 *		16-bit value of the register. 
 */
uint16_t twiReadRegister16(uint8_t slave_address, uint8_t register_address ) {
	uint16_t dataMSB;
	uint8_t dataLSB;
	twiStart(TWI_WRITE(slave_address));
	twiSend(register_address);
	twiStart(TWI_READ(slave_address));
	dataMSB = twiReadAck();
	dataLSB = twiReadNoAck();
	twiStop();
	dataMSB <<= 8;
	return dataMSB|dataLSB;
}

/*
 *	twiWriteRegister8: Writes data on an 8-bit register of the device. Works with 8-bit address.
 *
 *	Requires a working TWI interface.
 */
void twiWriteRegister8( uint8_t slave_address, uint8_t register_address, uint8_t register_value ) {
	twiStart(TWI_WRITE(slave_address));
	twiSend(register_address);
	twiSend(register_value);
	twiStop();
}

/*
 *	twiWriteRegister: Writes data on given 16-bit register of the device. Works with 8-bit address.
 *
 *	Requires a working TWI interface.
 */
void twiWriteRegister16( uint8_t slave_address, uint8_t register_address, uint16_t register_value ) {
	uint8_t dataMSB = (uint8_t) (register_value>>8);
	uint8_t dataLSB = (uint8_t) (register_value);
	twiStart(TWI_WRITE(slave_address));
	twiSend(register_address);
	twiSend(dataMSB);
	twiSend(dataLSB);
	twiStop();
}