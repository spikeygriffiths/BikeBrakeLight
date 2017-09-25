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
#include "log.h"
#include "UARTmod.h"
#include "OSCLI.h"
#include "ACCELmod.h"
#include "LEDmod.h"

static U8 btn, oldBtn;

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
		OSIssueEvent(EVENT_TICK, 1);	// Could give the arg in elapsed ms once we have a timer...
	}
}

void _OSIssueEvent(Event eventId, U32 eventArg)
{
	OSEventHandler(eventId, eventArg);
	LOGEventHandler(eventId, eventArg);
	UARTEventHandler(eventId, eventArg);
	ACCELEventHandler(eventId, eventArg);
	LEDEventHandler(eventId, eventArg);
	// Other event handlers here...
}

void OSEventHandler(Event event, U32 eventArg)
{
	switch (event) {
	case EVENT_PREINIT:
		DDRB = 0xE7;	// PB0-3 SPI (SS, MOSI and SCLK as outputs), PB5-7 main LEDs
		DDRD = 0x00;	// PD0 btn, PD2,3 WAKE from accel
		DDRE = 0x00;	// PE6 STAT from battery charger
		DDRF = 0x21;	// PF0 Indicator LED, PF4 Light lvl, PF5 Light Enable, PF6 Battery Voltage
		PORTB = 0x00;	// Turn off all main LEDs
		PORTF = 0x00;	// Indicator LED off and LDR disable
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
		break;
	case EVENT_POSTINIT:
		wdt_enable(WDTO_500MS);
		OSprintf("Spikey Bike Light for ATmega32U4 v0.1\r\n");
		break;
	case EVENT_TICK:
		wdt_reset();
		btn = BTN;	// Read button
		if (btn != oldBtn) {
			oldBtn = btn;
			OSIssueEvent(EVENT_BUTTON, btn);
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

U16 LDRget(void)
{
	U16 lightLvl = 0;
	
	DIDR0 |= 0x10;	// Disable Digital input for ADC4
	DDRF &= ~LGHTLVL_EN;	// Enable LDR by making EN an input
	ADMUX = 0x04;	// Select ADC for port ADC4, right-justified result (ADLAR=0)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescaler
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish a second time to get a real reading
	lightLvl = ADCL | (ADCH << 8);	// Sample 10-bit value
	//ADCSRA = 0x00;	// Shut down ADC to save power
	//PORTF &= ~LGHTLVL_EN;	// Disable LDR to save power
	lightLvl = 1024-lightLvl;	// Flip it round, since it's wired to give large readings when dark and low ones when bright
	return lightLvl;
}

U16 TMPget(void)
{
	U16 tempLvl = 0;
	ADMUX = 0xC7;	// Select ADC for Temperature, right-justified result, using internal 2.56V ref
	ADCSRB = 0x20;	// Set bit 5 to select Temperature (MUX is spread across two registers)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
//	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	while (0==(ADCSRA & 0x10)) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	// Poll ADC again to get actual value
//	while (ADCSRA & 0x40) ;	// Wait for conversion to finish (using ADSC bit going low)
	while (0==(ADCSRA & 0x10)) ;	// Wait for conversion to finish (using ADIF bit going high)
	tempLvl = ADCL | (ADCH << 8);	// Sample value (must read ADCL first)
	//ADCSRA = 0x00;	// Shut down ADC to save power
	return tempLvl;
}

