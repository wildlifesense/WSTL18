/********************************************************************************

  rtc.c: Header file for real time counter module of WSTL18 firmware.
 
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
#ifndef RTC_H_
#define RTC_H_

void rtcStart(void);				// Initialize and start the Real Time Counter.
void rtcStop(void);		// Stop the Real Time Counter.

void rtcIntervalCounterIncrement(void);			// Increment count of eight seconds interval
uint32_t rtcIntervalCounterGet(void);			// Get count of eight seconds interval
void rtcIntervalCounterClear(void);				// Clear count of eight seconds interval

#endif /* RTC_H_ */