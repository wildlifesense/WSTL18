/*
 * indicator.h
 *
 * Created: 07/02/2018 23:43:46
 *  Author: Nikos
 */ 


#ifndef INDICATOR_H_
#define INDICATOR_H_


void indicatorInitialize(void);
void indicatorOn(void);
void indicatorOff(void);
void indicatorShortBlink(void);		// Emit the shortest, barely visible, light pulse. Indicates normal operation.
void indicatorDoubleBlink(void);	// Emit a clear double pulse. Indicates error in operation.

#endif /* INDICATOR_H_ */