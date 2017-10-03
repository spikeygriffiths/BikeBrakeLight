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

static U8 btn;	//, oldBtn;
bool btnInt = false;
static U16 elapsedS = 0;	// Keep track of time spent active (don't know why yet, though...)

ISR(INT0_vect)	// Button edge detected
{
	btn = BTN;	// Read button
	btnInt = true;
}

int main(void)
{
	U16 ms, elapsedMs = 0;
	OSIssueEvent(EVENT_PREINIT, 0);
	OSIssueEvent(EVENT_INIT, 0);
	OSIssueEvent(EVENT_POSTINIT, 0);
	sei();	// Enable interrupts
	for (;;) {
		ms = 1;	// Should set this up correctly once we have a timer...
		OSIssueEvent(EVENT_TICK, ms);
		elapsedMs += ms;
		if (elapsedMs >= MS_PERSEC) {
			OSIssueEvent(EVENT_SECOND, elapsedMs / MS_PERSEC);
			elapsedS += elapsedMs / MS_PERSEC;
			elapsedMs %= MS_PERSEC;	// Keep remainder
		}
	}
}

void _OSIssueEvent(Event eventId, U16 eventArg)
{
	OSEventHandler(eventId, eventArg);
#ifdef SWITCH_LOG
	LOGEventHandler(eventId, eventArg);
#endif //def SWITCH_LOG
#ifdef SWITCH_UART
	UARTEventHandler(eventId, eventArg);
#endif //def SWITCH_UART
	ACCELEventHandler(eventId, eventArg);
	LEDEventHandler(eventId, eventArg);
	// Other event handlers here...
}

void OSEventHandler(Event event, U16 eventArg)
{
	bool sleepReq;

	switch (event) {
	case EVENT_PREINIT:
		PORTB = 0x10;	// Pull up PB5 (unused input)
		DDRB = 0xE7;	// PB0-3 SPI (SS, MOSI and SCLK as outputs), PB5-7 main LEDs
		PORTC = 0xC0;	// Pull up PC6,7 (unused inputs)
		DDRC = 0x00;	// Unused, only PC6,7 available
		PORTD = 0xF2;	// Pull up PD1,4,5,6,7 (unused inputs)
		DDRD = 0x00;	// PD0 btn, PD2,3 WAKE from accel
		PORTE = 0x04;	// PE2 is unused, so pullup to reduce power
		DDRE = 0x00;	// PE6 STAT from battery charger
		PORTF = 0x82;	// Pull up PF1,7 as they're unused
		DDRF = 0x21;	// PF0 Indicator LED, PF4 Light lvl, PF5 Light Enable, PF6 Battery Voltage
		PORTB = 0x00;	// Turn off all main LEDs
		PORTF = 0x00;	// Indicator LED off and LDR disable
		break;
	case EVENT_INIT:
		EICRA = 0x11;	// Wake on either edge from button (INT0) and accelerometer (INT2), as long as EIMSK says so...
		EIMSK = 0x05;	// Enable interrupts from button (INT0) and accelerometer (INT2)
		break;
	case EVENT_POSTINIT:
		wdt_enable(WDTO_500MS);
		OSprintf("\r\nReset Source 0x%2x\r\n", MCUSR);
		MCUSR = 0;	// Ready for next time
		OSprintf("Spikey Bike Light for ATmega32U4 v0.2\r\n");
		break;
	case EVENT_TICK:
		wdt_reset();
		if (btnInt) {	// This test could be in EVENT_WAKE, but it wouldn't be able to print anything then...
			btnInt = false;	// So that we don't continue to trigger
			OSprintf("Button Interrupt!\r\n");
			OSIssueEvent(EVENT_BUTTON, btn);	// btn value set from ISR
		}
#if 0
		btn = BTN;	// Read button from foreground
		if (btn != oldBtn) {
			oldBtn = btn;
			OSprintf("Button Poll!\r\n");
			OSIssueEvent(EVENT_BUTTON, btn);
		}
#endif
		sleepReq = true;	// Assume can sleep, unless any responder sets this to false
		OSIssueEvent(EVENT_REQSLEEP, &sleepReq);	// Request that system be allowed to sleep
		if (sleepReq) OSSleep();
		break;
	case EVENT_SECOND:
		OSprintf("sec...");
		break;
	case EVENT_REQSLEEP:
		if (btn) *(bool*)eventArg = false;	// Disallow sleep while button held down
		break;
	case EVENT_SLEEP:
		wdt_disable();	// Don't need a watchdog when were asleep
		PRR0 = 0xAD;	// Shut down TWI, Timer0, Timer1, SPI and ADC
		PRR1 = 0x99;	// Shut down USB, Timer3, Timer4 and USART
		break;
	case EVENT_WAKE:
		PRR0 = 0xA0;	// Re-enable Timer1, SPI and ADC.  Timer1 used by LEDs for PWM, SPI used by Accelerometer, ADC used by LDR & BattLvl
		//PRR1 = 0x00;	// Re-enable USB, Timer3, Timer4 and USART
		wdt_enable(WDTO_500MS);
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

void OSSleep(void)
{
	OSprintf("Sleep\r\n");
	OSIssueEvent(EVENT_SLEEP,0);	// Tell system we're going to sleep
    SMCR = 0x5;	// Power Down (----010x) and Sleep Enable bit (----xxx1) and only wake on external interrupt (button, accelerometer or USB power)
    sleep_cpu();    // Cause AVR to enter sleep mode
    SMCR = 0x0;	// Clear Sleep Enable bit (----xxx0)
	OSIssueEvent(EVENT_WAKE,0);	// Tell system we're awake
	//OSprintf("Wake!\r\n");	// No earlier than this - UART won't be operating until WAKE has finished
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
	ADCSRA = 0x00;	// Shut down ADC to save power
	PORTF &= ~LGHTLVL_EN;	// Disable LDR to save power
	lightLvl = 1024-lightLvl;	// Flip it round, since it's wired to give large readings when dark and low ones when bright
	return lightLvl;
}

U16 TMPget(void)
{
	U16 tempLvl = 0;
	ADMUX = 0xC7;	// Select ADC for Temperature, right-justified result, using internal 2.56V ref
	ADCSRB = 0x20;	// Set bit 5 to select Temperature (MUX is spread across two registers)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	// Poll ADC again to get actual value
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish (using ADSC bit going low)
	tempLvl = ADCL | (ADCH << 8);	// Sample value (must read ADCL first)
	ADCSRA = 0x00;	// Shut down ADC to save power
	return tempLvl;
}

