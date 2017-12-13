/*
 * USBmod.c
 *
 * Created: 06/12/2017 10:06:05
 *  Author: spikey
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "main.h"
#include "log.h"
#include "LEDmod.h"
#include "UARTmod.h"

static bool usbInt = false;
static bool usbState;

ISR(USB_GEN_vect)	// USB state change.  See C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8\avr8-gnu-toolchain\avr\include\avr\iom32u4.h
{
	USBINT = 0x00;	// Clear interrupt
	usbInt = true;	// Tell foreground
}

void USBEventHandler(Event event, U16 eventArg)
{
	switch (event) {
	case EVENT_INIT:
		USBCON = 0x11;	// Enable USB power detection (but not the USB controller).  NB Doesn't work!  :-(
		break;
	case EVENT_POSTINIT:
		usbState = (0 != (USBSTA & 0x01));	// State (true if attached, false if detached). Could probably assume attached after reset in real life, unless we expect watchdog resets...
		break;
	case EVENT_TICK:
		if (usbInt) {
			usbInt = false;	// Acknowledge software interrupt
			usbState = (0 != (USBSTA & 0x01));	// State (true if attached, false if detached)
			OSprintf("USB %s%s", (usbState) ?"ATTached" : "DETached", OS_NEWLINE);
			OSIssueEvent(EVENT_USB, usbState);
		}
		break;
	case EVENT_USB:
		if (eventArg) USB_LED_ON; else USB_LED_OFF;	// Indicator LED shows USB state
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}	// end switch(event)
}

