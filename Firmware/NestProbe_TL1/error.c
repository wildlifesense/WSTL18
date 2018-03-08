/********************************************************************************

  error.c: Error checking and flagging for the NestProbe TL1 firmware.
 
  Copyright (C) 2017 Nikos Vallianos <nikos@wildlifesense.com>
  
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
#include "error.h"

uint16_t error_flags;					// Flags to indicate status of device. POR defaults to 0x00.

void errorInitFlags(void) {
	error_flags = 0;
}
/*
 * Set a flag on this module's flags variable. Takes one of ERROR_FLAG_... #defines.
 */
void errorSetFlag(uint16_t flag_to_set) {
	error_flags |= (1<<flag_to_set);
}

void errorClearFlag(uint16_t flag_to_clear) {
	error_flags &= ~(1<<flag_to_clear);
}

void errorClearAll(void) {
	errorInitFlags();
}

uint16_t errorGetFlags(void) {
	return error_flags;
}