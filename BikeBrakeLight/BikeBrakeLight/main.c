/*
 * BikeBrakeLight.c
 *
 * Created: 26/07/2017 22:04:57
 * Author : Spikey
 */ 

#include <avr/io.h>
#include <avr/wdt.h>	// Watchdog timer
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "main.h"

static U8 btn, oldBtn;
static U8 led;

void LEDpwm(U8 fractionA, U8 fractionB, U8 fractionC);

// Only use Pin Change Interrupt handler for devices supporting this.
#ifdef EXAMPLE_PCINT_vect
ISR(EXAMPLE_PCINT_vect)
{
//	btn = PIND & 0x01;	// Read button
}
#endif

int main(void)
{
	OSIssueEvent(EVENT_PREINIT, 0);
	OSIssueEvent(EVENT_INIT, 0);
	OSIssueEvent(EVENT_POSTINIT, 0);
	//sei();	// Enable interrupts
	for (;;) {
		OSIssueEvent(EVENT_TICK, 0);	// Could give the arg in elapsed ms once we have a timer...
	}
}

void _OSIssueEvent(Event event, U32 eventArg)
{
	OSEventHandler(event, eventArg);
	// Other event handlers here...
}

void OSEventHandler(Event event, U32 eventArg)
{
	switch (event) {
	case EVENT_PREINIT:
		DDRB = 0xE0;	// PB0-3 SPI, PB5-7 main LEDs
		DDRD = 0x00;	// PD0 btn, PD2,3 WAKE from accel
		DDRE = 0x00;	// PE6 STAT from battery charger
		DDRF = 0x21;	// PF0 Indicator LED, PF4 Light lvl, PF5 Light Enable, PF6 Battery Voltage
		PORTB = 0x00;	// Turn off all main LEDs
		PORTF = 0x21;	// Indicator LED and Light sensor enable
		LEDpwm(0x10, 0x10, 0x10);	// All LEDs 1/16th bright
		TCCR1A = 0xA9;	// Bits 2-3,4-5 & 6-7 are PWM compare for each main LED, 10=Mark then space, bits 0-1 are WGM0,1
		TCCR1B = 0x09;	// Bits 0-2 select clock (001==No prescaler), bits 3-4 are WGM2,3 (0101 for WGM=Fast PWM, 8 bit)
		//TCCR1C = 0x00;	// Only used for input capture, so either leave or set to zero
		break;
	case EVENT_INIT:
		// Only use Pin Change Interrupt handler for devices supporting this.
#ifdef EXAMPLE_PCICR
		/* Enable pin change interrupt for PB0 which is controlled by SW0
		 *
		 * First we need to enable pin change interrupt for the wanted port.
		 */
//		EXAMPLE_PCICR = (1 << EXAMPLE_PCIE);

		// Then we need to set the pin change port mask to get the bit we want.
//		EXAMPLE_PCMSK = (1 << PCINT0);
#endif
		led = 5;
		break;
	case EVENT_POSTINIT:
		wdt_enable(WDTO_500MS);
		break;
	case EVENT_TICK:
		wdt_reset();
		btn = BTN;	// Read button
		if (btn != oldBtn) {
			oldBtn = btn;
			if (btn) {
				led++;
				if (led > 7) led = 5;	// cycle 5 -> 6 -> 7, then back to 5
				switch (led) {
				case 5:
					LEDpwm(0x40, 0x10, 0x10);	// All LEDs 1/16th bright
					break;
				case 6:
					LEDpwm(0x10, 0x40, 0x10);	// All LEDs 1/16th bright
					break;
				case 7:
					LEDpwm(0x10, 0x10, 0x40);	// All LEDs 1/16th bright
					break;
				}
				//PORTB |= 1 << led;
				PORTF |= IND_LED;
			} else {
				LEDpwm(0x10, 0x10, 0x10);	// All LEDs 1/16th bright
				PORTF &= ~IND_LED;
			}
		}
		break;
	case EVENT_SLEEP:
		sleep_enable();
		sleep_mode();	// Cause AVR to enter sleep mode
		break;
	case EVENT_WAKE:
		sleep_disable();
		break;
	}
}

void LEDpwm(U8 fractionA, U8 fractionB, U8 fractionC)
{
		OCR1AL = fractionA;
		OCR1BL = fractionB;
		OCR1CL = fractionC;
}