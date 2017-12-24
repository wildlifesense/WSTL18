/********************************************************************************

  max30205.h: Header file for the MAX30205 temperature sensor module of WSTL18 firmware.
 
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
#ifndef MAX30205_H_
#define MAX30205_H_

// OS is pin D2.

#define MAX30205_ADDRESS	0x90					// When A0, A1, and A2 are all GND (DS p.9)

/*
 *	The device contains four registers, each of which consists of two bytes. The configuration
 *	register contains only 1 byte of data and, when read as a two-byte register, repeats the same
 *	data for the second byte (DS p.10).
 *	During reads and writes the pointer register auto-increments but does not wrap from 0x03 to 0x00.
 */
#define MAX30205_REG_TEMP	0x00	//	Read-only, POR value 0 deg.
#define MAX30205_REG_CONF	0x01	//	R/W, POR value null.
#define MAX30205_REG_HYST	0x02	//	R/W, POR value 75 deg.
#define MAX30205_REG_TOS	0x03	//	R/W, POR value 80 deg.

#define MAX30205_CONF_SHUTDOWN		0	// Shutdown bit in configuration register.
#define MAX30205_CONF_INTERRUPT		1	// Interrupt bit in configuration register. 0: Comparator, 1: Interrupt
#define MAX30205_CONF_OSPOLARITY	2	// OS polarity bit in configuration register. 0: OS active low, 1: OS active high
#define MAX30205_CONF_FAULTQUEUE	3	// Number of faults to trigger an OS condition.
// Previous setting is two-byte.
#define MAX30205_CONF_DATAFORMAT	5	// Data format for temperature, Tos and Thyst. 0: Normal, 1: Extended
#define MAX30205_CONF_TIMEOUT		6	// Reset I2C when SDA low > 50ms. 0: Enable, 1: Disable.
#define MAX30205_CONF_ONESHOT		7	// Do a one-shot from shutdown: 0: Nothing, 1: Do it.

#define MAX30205_CONF_DEFAULT		0b00000000	// Default configuration register after power-on reset.

void		max30205Init(void);
void		max30205Shutdown(void);
void		max30205Resume(void);
void		max30205SaveConfiguration(void);
uint8_t		max30205ReadConfiguration(void);
void		max30205LoadConfiguration(void);
void		max30205StartOneShot(void);
uint16_t	max30205ReadTemperature(void);

#endif /* MAX30205_H_ */