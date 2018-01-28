/********************************************************************************

  max30205.c: MAX30205 temperature sensor module of WSTL18 firmware.
 
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
#include "twi.h"
#include "max30205.h"
#include "uart.h"

// !!!!!! Max first conversion time after POR is 50ms (DS p.2 bottom).
/*
 *	max30205_configuration_register: Always contains the current configuration register. All
 *	functions in this module write to this register and then send it to the device. This eliminates
 *	the need to read the register from the device before making changes, except when reading the
 *	fault queue.
 */
uint8_t max30205_config;

/*
 * Initialize MAX30205 sensor.
 */
void max30205Init(void) {
	DDRD &= ~(1<<DDRD2); PORTD |= (1<<PORTD2);		// Set D2 as input and pull-up.
	// No config setup; default is good.
	twiEnable();
	max30205EnterShutdown();
	twiDisable();
}


void max30205EnterShutdown(void) {
	max30205ReadConfig();
	max30205_config |= 1<<MAX30205_CONF_SHUTDOWN;
	max30205SaveConfig();
	twiDWrite8(MAX30205_ADDRESS, MAX30205_REG_CONF, max30205_config);
}



/*
 * max30205ReadConfig: Read the configuration register into module variable and also return it.
 */
uint8_t max30205ReadConfig(void) {
	max30205_config = twiDRead8(MAX30205_ADDRESS, MAX30205_REG_CONF);
	return max30205_config;
}

/*
 *	max30205SaveConfiguration: Takes the configuration register variable from this module
 *	and stores it in the temperature sensor.
 */
void max30205SaveConfig(void) {
	twiDWrite8(MAX30205_ADDRESS, MAX30205_REG_CONF, max30205_config);
}



/*
	max30205StartOneShot: Trigger a one-shot on the temperature sensor while it's in shutdown mode.
	This function only triggers the temperature conversion. Because it takes a considerable time
	until the results are ready, it will in the future be useful to disable the TWI, put the mcu
	in power save, wait for the conversion time to pass, then wake the mcu up, enable the TWI, and
	finally read the temperature.
	For now, the mcu will only use _delay_ms, but I'm keeping the Start and Read functions separate
	so they're in place for the above power-saving method to be implemented.
	
	It is the job of a higher-level module to control max30205StartOneShot and max30205ReadTemperature
	for optimization. This module only offers these two functions and nothing above them.
	
*/
void max30205StartOneShot(void) {
	if(max30205_config & (1<<MAX30205_CONF_SHUTDOWN)) {	// One-shot only works from shut-down mode. Ignored otherwise.
		max30205_config |= (1<<MAX30205_CONF_ONESHOT);	// Set one-shot bit in configuration variable
		twiEnable();
		max30205SaveConfig();
		twiDisable();
		max30205_config &= ~(1<<MAX30205_CONF_ONESHOT);	// Only unset ONESHOT in variable, sensor auto-resets after one-shot completes.
	} // else: some sort of programming fault?
	// While in shutdown, the I2C interface remains active and all registers remain
	// accessible to the master.
	// The results of a conversion are available to read after 50ms.
	// Max conversion time is 50ms.
}

/*
	max30205ReadTemperature: Read the current temperature from the sensor. 
	If reading after a one-shot, wait for 50ms before reading.
 
	Returns:
		16-bit temperature reading as given by the device. Actual temperature depends on settings.

	It is the job of a higher-level module to control max30205StartOneShot and max30205ReadTemperature
	for optimization. This module only offers these two functions and nothing above them.
 */
uint16_t max30205ReadTemperature(void) {
	uint16_t temperature;
	twiEnable();
	temperature = twiDRead16(MAX30205_ADDRESS, MAX30205_REG_TEMP);
	twiDisable();
	return temperature;
}