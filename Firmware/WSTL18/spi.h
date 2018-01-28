/********************************************************************************

  spi.h: Header file for the SPI module of WSTL18 firmware.
 
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
#ifndef SPI_H_
#define SPI_H_

void    spiEnable(void);
void	spiDisable(void);						// Stop SPI and set for low power consumption.

uint8_t	spiTradeByte(uint8_t byte);

void    spiExchangeArray(uint8_t * dataout, uint8_t * datain, uint8_t len);
void	spiTransmitArray(uint8_t * dataout, uint8_t len);
void	spiReceiveArray(uint8_t * datain, uint8_t len);

#endif /* SPI_H_ */