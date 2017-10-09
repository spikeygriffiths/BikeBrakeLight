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

static bool btnInt = false;
static bool btnState;	// true when down
static U16 btnDnTimerMs;	// Time of button being held down (will wrap after a minute)
static U16 elapsedS = 0;	// Keep track of time spent active (don't know why yet, though...)

ISR(INT0_vect)	// Button edge detected
{
	btnState = (0 != BTN);	// Read button as true when down
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
		PORTB = 0x1E;	// Pull up PB5 (unused input) as well as MOSI, MISO and SLCK when they're not used for SPI
		DDRB = 0xE1;	// PB0-3 SPI (SS as output, MOSI, MISO and SCLK as inputs until SPI activated), PB5-7 main LEDs
		PORTC = 0xC0;	// Pull up PC6,7 (unused inputs)
		DDRC = 0x00;	// Unused, only PC6,7 available
		PORTD = 0xF2;	// Pull up PD1,4,5,6,7 (unused inputs)
		DDRD = 0x00;	// PD0 btn, PD2,3 WAKE from accel
		PORTE = 0x04;	// PE2 is unused, so pullup to reduce power
		DDRE = 0x00;	// PE6 STAT from battery charger
		PORTF = 0x82;	// Pull up PF1,7 as they're unused
		DDRF = 0x21;	// PF0 Indicator LED, PF4 Light lvl, PF5 Light Enable, PF6 Battery Voltage
		PORTB = 0x00;	// Turn off all main LEDs
		PORTF = 0x50;	// Indicator LED off and LDR disable, but pull LDR_VAL and BATT_VAL up when not in use as ADC
		DIDR0 = 0x50;	// Digital Input Disable Register for ADC from LDR and Batt.  See 7.8.6 and 24.9.5
		break;
	case EVENT_INIT:
		EICRA = 0x11;	// Wake on either edge from button (INT0) and accelerometer (INT2), as long as EIMSK says so...
		EIMSK = 0x05;	// Enable interrupts from button (INT0) and accelerometer (INT2)
		break;
	case EVENT_POSTINIT:
		wdt_enable(WDTO_500MS);
		OSprintf("\r\nReset Source 0x%2x\r\n", MCUSR);
		MCUSR = 0;	// Ready for next time
		OSprintf("Spikey Bike Light for ATmega32U4 v0.3\r\n");
		break;
	case EVENT_TICK:
		wdt_reset();
		if (btnState) btnDnTimerMs += eventArg;	// Time how long button stays down for
		if (btnInt) {	// This test could be in EVENT_WAKE, but it wouldn't be able to print anything then...
			btnInt = false;	// So that we don't continue to trigger
			OSprintf("Button Interrupt!\r\n");
			OSIssueEvent(EVENT_BUTTON, btnState);	// btnState value set from ISR
		}
		// Try to sleep...
		sleepReq = true;	// Assume can sleep, unless any responder sets this to false
		OSIssueEvent(EVENT_REQSLEEP, &sleepReq);	// Request that system be allowed to sleep
		if (sleepReq) OSSleep(SLEEPTYPE_LIGHT);	// Allow either button or accelerometer to wake us from sleep
		break;
	case EVENT_BUTTON:
		btnState = eventArg;
		if (btnState) {
			IND_LED_ON;
			btnDnTimerMs = 0;	// Start timer if going down
		} else {
			IND_LED_OFF;
			if (btnDnTimerMs > 1000)	{	// Read timer if going up
				OSSleep(SLEEPTYPE_DEEP);	// Sleep, only looking for button to wake
			} else {
				OSIssueEvent(EVENT_NEXTLED, NEXTLED_BUTTON);	// Select next light pattern based on button press
			}
		}
		break;
	case EVENT_SECOND:
		OSprintf("sec...");
		break;
	case EVENT_REQSLEEP:
		if (btnState) *(bool*)eventArg = false;	// Disallow sleep while button held down
		break;
	case EVENT_SLEEP:
		wdt_disable();	// Don't need a watchdog when we're asleep
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

void OSSleep(int sleepType)
{
	if (SLEEPTYPE_DEEP == sleepType) {	// So only allow button to wake us up
		OSprintf("DeepSleep\r\n");
		EICRA = 0x01;	// Wake on either edge from button (INT0), as long as EIMSK says so...
		EIMSK = 0x01;	// Enable interrupts from button (INT0)
	} else {	// Assume SLEEPTYPE_LIGHT, so allow button and accelerometer to wake us up
		OSprintf("LightSleep\r\n");
		EICRA = 0x11;	// Wake on either edge from button (INT0) and accelerometer (INT2), as long as EIMSK says so...
		EIMSK = 0x05;	// Enable interrupts from button (INT0) and accelerometer (INT2)
	}
	OSIssueEvent(EVENT_SLEEP, sleepType);	// Tell system we're going to sleep
    SMCR = 0x5;	// Power Down (----010x) and Sleep Enable bit (----xxx1) and only wake on external interrupt (button, accelerometer or USB power)
    sleep_cpu();    // Cause AVR to enter sleep mode
    SMCR = 0x0;	// Clear Sleep Enable bit (----xxx0)
	OSIssueEvent(EVENT_WAKE, sleepType);	// Tell system we've woken from sleep
	//OSprintf("Wake!\r\n");	// No earlier than this - UART won't be operating until WAKE has finished
}

U16 LDRget(void)
{
	U16 lightLvl = 0;
	
	DIDR0 |= LDR_VAL;	// Disable Digital input for ADC4
	//DDRF &= ~LDR_EN;	// Enable LDR by making EN an input
	PORTF |= LDR_EN;	// Enable LDR by setting enable line high
	ADMUX = 0x04;	// Select ADC for port ADC4, right-justified result (ADLAR=0)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescaler
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish a second time to get a real reading
	lightLvl = ADCL | (ADCH << 8);	// Sample 10-bit value
	ADCSRA = 0x00;	// Shut down ADC to save power
	DDRF |= LDR_EN;	// Make LDR_EN an output
	PORTF &= ~LDR_EN;	// Disable LDR to save power
	DIDR0 &= ~LDR_VAL;	// Re-enable Digital input for ADC4
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

