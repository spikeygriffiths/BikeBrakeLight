/*
 * BikeBrakeLight.c
 *
 * Created: 26/07/2017 22:04:57
 * Author : Spikey
 */ 

#include <avr/io.h>
#include <avr/wdt.h>	// Watchdog timer (actually in C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8\avr8-gnu-toolchain\avr\include\)
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "main.h"
#include "log.h"
#include "UARTmod.h"
#include "OSCLI.h"
#include "ACCELmod.h"
#include "LEDmod.h"
#include "BTNmod.h"
#include "ADCmod.h"
#include "BATmod.h"

static bool usbInt = false;
static bool usbState;
static U16 elapsedS = 0;	// Keep track of time spent active (don't know why yet, though...)
static U8 oldMCUSR;	// Reset source
static char* resetStr;

static const char BrownOut[] = "BrownOut";
static const char WatchDog[] = "WatchDog";
static const char ExtReset[] = "ExtReset";
static const char PowerOn[] = "PowerOn";
static const char Unknown[] = "Unknown";

ISR(USB_GEN_vect)	// USB state change.  See C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8\avr8-gnu-toolchain\avr\include\avr\iom32u4.h
{
	USBINT = 0x00;	// Clear interrupt
	//usbState = (0 != (USBSTA & 0x01));	// New state (true if attached, false if detached)
	usbInt = true;	// Tell foreground
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
	ADCEventHandler(eventId, eventArg);
	BATEventHandler(eventId, eventArg);
	BTNEventHandler(eventId, eventArg);
	// Other event handlers here...
}

void OSEventHandler(Event event, U16 eventArg)
{
	bool sleepReq;

	switch (event) {
	case EVENT_PREINIT:
		oldMCUSR = MCUSR;
		MCUSR = 0;	// Ready for next time
		PORTB = 0x10;	// Pull up PB5 (unused input).  Was 0x1E to pull up MOSI, MISO and SLCK when they're not used for SPI, but then SPI stopped working...
		DDRB = 0xE7;	// PB0-3 SPI (MOSI, SCLK and SS as outputs,  MISO as inputs to allow SPI to be activated), PB5-7 main LEDs
		PORTC = 0xC0;	// Pull up PC6,7 (unused inputs)
		DDRC = 0x00;	// Unused, only PC6,7 available
		PORTD = 0xF2;	// Pull up PD1,4,5,6,7 (unused inputs)
		DDRD = 0x00;	// PD0 btn IRQ, PD2,3 WAKE IRQs from ADXL363 accelerometer
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
		USBCON = 0x11;	// Enable USB power detection (but not the USB controller macro)
		break;
	case EVENT_POSTINIT:
		wdt_enable(WDTO_500MS);
		if (oldMCUSR & (1<<BORF)) {	// Brown Out
			resetStr = (char*)BrownOut;
		} else if (oldMCUSR & (1<<WDRF)) {
			resetStr = (char*)WatchDog;
		} else if (oldMCUSR & (1<<EXTRF)) {
			resetStr = (char*)ExtReset;
		} else if (oldMCUSR & (1<<PORF)) {
			resetStr = (char*)PowerOn;
		} else resetStr = (char*)Unknown;
		OSprintf("\r\n\nReset:%s (0x%2x)\r\n", resetStr, oldMCUSR);
		OSprintf("%s%s", OS_BANNER, OS_NEWLINE);
		usbState = (0 != (USBSTA & 0x01));	// State (true if attached, false if detached)
		sysBits = (DAYTIME_UNKNOWN << SYSBITNUM_DAY) | SYSBIT_STATIONARY | ((usbState & 1) << SYSBITNUM_USB);
		break;
	case EVENT_TICK:
		wdt_reset();
		if (usbInt) {
			usbInt = false;	// Acknowledge software interrupt
			usbState = (0 != (USBSTA & 0x01));	// State (true if attached, false if detached)
			OSprintf("USB %s%s", (usbState)?"attached" : "detached", OS_NEWLINE);
			OSIssueEvent(EVENT_USB, usbState);
		}
		// Try to sleep...
		sleepReq = true;	// Assume can sleep, unless any responder sets this to false
		OSIssueEvent(EVENT_REQSLEEP, &sleepReq);	// Request that system be allowed to sleep
		if (sleepReq) OSSleep(SLEEPTYPE_LIGHT);	// Allow either button or accelerometer to wake us from sleep
		break;
	case EVENT_DOUBLE_CLICK:
		OSIssueEvent(EVENT_INFO, 0);	// For now, anyway.  Could use some other trigger
		break;
	case EVENT_LONG_CLICK:
		OSSleep(SLEEPTYPE_DEEP);	// Sleep, only looking for button press to wake
		break;
	case EVENT_USB:
		sysBits = (sysBits & ~SYSBITMASK_USB) | ((eventArg & 1) << SYSBITNUM_USB);
		break;
	case EVENT_DAYLIGHT:
		sysBits = (sysBits & ~SYSBITMASK_DAY) | ((eventArg & 3) << SYSBITNUM_DAY);
		break;
	case EVENT_SLEEP:
		wdt_disable();	// Don't need a watchdog when we're asleep
		IND_LED_OFF;
		PRR0 = 0xAD;	// Shut down TWI, Timer0, Timer1, SPI and ADC
		PRR1 = 0x99;	// Shut down USB, Timer3, Timer4 and USART
		break;
	case EVENT_WAKE:
		PRR0 = 0xA0;	// Re-enable Timer1, SPI and ADC.  Timer1 used by LEDs for PWM, SPI used by Accelerometer, ADC used by LDR & BattLvl
		PRR1 = 0x00;	// Re-enable USB, Timer3, Timer4 and USART
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
		EICRA = 0x03;	// Wake on down edge from button (INT0), as long as EIMSK says so...
		EIMSK = 0x01;	// Enable interrupts from button (INT0)
	} else {	// Assume SLEEPTYPE_LIGHT, so allow button and accelerometer to wake us up
		OSprintf("LightSleep\r\n");
		EICRA = 0x13;	// Wake on down edge from button (INT0) and either edge from accelerometer (INT2), as long as EIMSK says so...
		EIMSK = 0x05;	// Enable interrupts from button (INT0) and accelerometer (INT2)
	}
	OSIssueEvent(EVENT_SLEEP, sleepType);	// Tell system we're going to sleep
    SMCR = 0x5;	// Power Down (----010x) and Sleep Enable bit (----xxx1) and only wake on external interrupt (button, accelerometer or USB power)
    sleep_cpu();    // Cause AVR to enter sleep mode
    SMCR = 0x0;	// Clear Sleep Enable bit (----xxx0)
	OSIssueEvent(EVENT_WAKE, sleepType);	// Tell system we've woken from sleep
	EICRA = 0x11;	// Wake on either edge from button (INT0) and accelerometer (INT2), as long as EIMSK says so...
	EIMSK = 0x05;	// Enable interrupts from button (INT0) and accelerometer (INT2)
	//OSprintf("Wake!\r\n");	// No earlier than this - UART won't be operating until WAKE has finished
}
