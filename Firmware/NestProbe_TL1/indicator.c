/*
 * indicator.c
 *
 * Created: 07/02/2018 23:43:28
 *  Author: Nikos
 */
#include <avr/io.h>
#include <util/delay.h>
#define INDICATOR_OUTPUT	DDRE |= (1<<DDRE1)					// Set the indicator pin as output
#define INDICATOR_ON		PORTE |= (1<<PORTE1)				// Turn the indicator on
#define INDICATOR_OFF		PORTE &= ~(1<<PORTE1)				// Turn the indicator off

void indicatorInitialize(void) {
	INDICATOR_OUTPUT;					// Set LED pin as output
	INDICATOR_OFF;				// Set it to LOW.
}

/*
 * Turn indicator on.
 */
void indicatorOn(void) {
	INDICATOR_ON;
}

/*
 * Turn indicator off.
 */
void indicatorOff(void) {
	INDICATOR_OFF;
}
/*
 * Emit the shortest, barely visible, light pulse. Indicates normal operation.
 */
void indicatorShortBlink(void) {
	INDICATOR_ON;
	_delay_ms(1);
	INDICATOR_OFF;}

/*
 * Emit a clear double pulse. Indicates error in operation.
 */
void indicatorDoubleBlink(void) {
	INDICATOR_ON;
	_delay_ms(5);
	INDICATOR_OFF;
	_delay_ms(200);
	INDICATOR_ON;
	_delay_ms(5);
	INDICATOR_OFF;
}