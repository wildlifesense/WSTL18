/*
 * error.h
 *
 * Created: 08/02/2018 01:09:58
 *  Author: Nikos
 */
#ifndef ERROR_H_
#define ERROR_H_

#define ERROR_FLAG_LOGGING			0		// Is the device currently logging? 0: No, 1: Yes
#define ERROR_FLAG_ERROR_MEM		1		// Encountered memory error?		0: No, 1: Yes
#define ERROR_FLAG_ERROR_TMP		2		// Encountered error with temperature sensor?	0: No, 1: Yes
#define ERROR_FLAG_RX_BUFFER_OVF	3		// Command received via uart overflowed buffer.
#define ERROR_FLAG_COMMAND_ERR		4		// General command receive error.
#define ERROR_FLAG_STH1				5		// Some flag
#define ERROR_FLAG_STH2				6

void errorInitFlags(void);
void errorSetFlag(uint16_t flag_to_set);
void errorClearFlag(uint16_t flag_to_clear);
uint16_t errorGetFlags(void);

#endif /* ERROR_H_ */