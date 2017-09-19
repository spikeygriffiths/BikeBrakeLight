/*
 * LEDmod.c
 *
 * Created: 19/09/2017 09:03:37
 *  Author: Spikey
 */ 
#include <avr/io.h>
//#include <stdio.h>
//#include <stdarg.h>
#include <stdbool.h>
#include "main.h"
#include "osstr.h"
#include "UARTmod.h"	// For OSprintf

static U8 led;
static U8 brt, dim;	// LED levels, calculated according to LDR
static U16 ambientLight;	// Not sure this should be here - could have LDRmod which could sample LDR and advertise it?----+++++++++++++++++-

// Private function prototypes
void LEDpwm(U8 fractionA, U8 fractionB, U8 fractionC);

void LEDEventHandler(U8 eventId, U16 eventArg)
{
	switch (eventId) {
	case EVENT_PREINIT:
		// Initialise LEDs
		TCCR1A = 0xA9;	// Bits 2-3,4-5 & 6-7 are PWM compare for each main LED, 10=Mark then space, bits 0-1 are WGM0,1
		TCCR1B = 0x09;	// Bits 0-2 select clock (001==No prescaler), bits 3-4 are WGM2,3 (0101 for WGM=Fast PWM, 8 bit)
		break;
	case EVENT_INIT:
		led = 5;
		brt = 0x40; dim = 0x00;
		LEDpwm(dim, dim, dim);	// All LEDs off
		break;
	case EVENT_BUTTON:
		ambientLight = LDRget();
		// Could OSIssueEvent(EVENT_LIGHTLEVEL, ambientLight);
		brt = ambientLight >> 8;
		dim = ambientLight >> 10;
		if (eventArg) {	// Button down
			OSprintf("LDR %d%%\r\n", ambientLight/10);
			led++;
			if (led > 7) led = 5;	// cycle 5 -> 6 -> 7, then back to 5
			switch (led) {
			case 5:
				LEDpwm(brt, dim, dim);
				break;
			case 6:
				LEDpwm(dim, brt, dim);
				break;
			case 7:
				LEDpwm(dim, dim, brt);
				break;
			}
			PORTF |= IND_LED;
		} else {
			LEDpwm(dim, dim, dim);
			PORTF &= ~IND_LED;
		}
		break;
	}
}

void LEDpwm(U8 fractionA, U8 fractionB, U8 fractionC)
{
	OCR1AL = fractionA;
	OCR1BL = fractionB;
	OCR1CL = fractionC;
}
