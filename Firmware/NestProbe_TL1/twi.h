/********************************************************************************

  twi.c: Header file for the two-wire interface (aka I2C) module of WSTL18 firmware.
 
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

#ifndef TWI_H_
#define TWI_H_

#define TWI_SLA_READ(TWI_ADDR)	TWI_ADDR|1
#define TWI_SLA_WRITE(TWI_ADDR)	TWI_ADDR|0

// sets pullups and initializes bus speed to 400kHz (at FCPU=8MHz)
void twiEnable(void);								// Enable TWI module.
void twiDisable(void);								// Disables the TWI module and stops its clock.

// Control
uint8_t twiStart(uint8_t address);					// Sends a start condition (sets TWSTA)
void _twiStartWait(uint8_t address);					// Sends a start condition and waits for ACK
void _twiSetStopCondition(void);									// Sends a stop  condition (sets TWSTO)

void _twiSend(uint8_t data);							// Loads data, sends it out, waiting for completion
void twiStream(uint8_t address, uint8_t *data);		// Send a null-terminated string to a TWI device. Expects a '\0' terminated string

uint8_t twiReadRegister8_DEPRECATED(uint8_t slave_address, uint8_t register_address);
uint8_t twiDRead8(uint8_t slave_address, uint8_t register_address);
uint16_t twiDRead16(uint8_t slave_address, uint8_t register_address);
void twiDWrite8(uint8_t slave_address, uint8_t register_address, uint8_t write_value);
uint16_t twiReadRegister16(uint8_t slave_address, uint8_t register_address );
void twiWriteRegister8( uint8_t slave_address, uint8_t register_address, uint8_t register_value );
void twiWriteRegister16( uint8_t slave_address, uint8_t register_address, uint16_t register_value );
//

uint8_t _twiReadAck(void);       // Read in from slave, sending ACK when done (sets TWEA)
uint8_t _twiReadNoAck(void);     // Read in from slave, sending NOACK when done (no TWEA)




#endif /* TWI_H_ */