/*
 * BTNmod.c
 *
 * Created: 09/10/2017 17:29:59
 *  Author: Spikey
 */ 

#include <avr/interrupt.h>
#include "main.h"
#include "LEDmod.h"	// For IND_LED_xx
#include "log.h"
#include "UARTmod.h"

static bool btnInt = false;
static bool btnDown;	// true when down
static U16 btnDnTimerMs;	// Time of button being held down, up to 1 second
static U16 btnUpTimerMs;	// Time of button being up, started when button released
static BtnState btnState;

ISR(INT0_vect)	// Button edge detected
{
	btnDown = (0 != BTN);	// Read button as true when down
	btnInt = true;
}

void BTNEventHandler(Event event, U16 eventArg)
{
	switch (event) {
	case EVENT_POSTINIT:
		btnState = BTNSTATE_IDLE;
		break;
	case EVENT_TICK:
		if (btnDown) {
			btnDnTimerMs += eventArg;	// Time how long button stays down for
			if (btnDnTimerMs > BTN_LONGMS)	{	// Read timer as button is held down
				OSSleep(SLEEPTYPE_DEEP);	// Sleep, only looking for button to wake
			}
		} else {	// Button up
			if (btnUpTimerMs < MS_PERSEC) {
				btnUpTimerMs++;	// Time first second of up time, to see if double-clicking
			} else btnState = BTNSTATE_IDLE;
		}
		break;
	case EVENT_WAKE:
		btnState = BTNSTATE_IDLE;
		if (btnInt) {
			btnInt = false;	// So that we don't continue to trigger
			OSIssueEvent(EVENT_BUTTON, btnDown);	// btnDown value set directly from ISR
		}
		break;
	case EVENT_BUTTON:
		if (eventArg) {
			IND_LED_ON;
			if ((btnUpTimerMs < BTN_CLICKMS) && (BTNSTATE_GAP == btnState))
				btnState = BTNSTATE_SECONDCLICK;
			else btnState = BTNSTATE_FIRSTCLICK;
			btnDnTimerMs = 0;	// Start timer if going down
		} else {
			IND_LED_OFF;
			if ((btnDnTimerMs < BTN_CLICKMS) && (BTNSTATE_SECONDCLICK == btnState)) {	// End of double click when button goes up 2nd time
				OSIssueEvent(EVENT_DOUBLE_CLICK, 0);
				btnState = BTNSTATE_IDLE;
			} else {
				btnState = BTNSTATE_IDLE;
			}
			if ((btnDnTimerMs < BTN_CLICKMS) && (BTNSTATE_FIRSTCLICK == btnState)) {
				btnState = BTNSTATE_GAP;
			} else btnState = BTNSTATE_IDLE;
			btnUpTimerMs = 0;
			OSIssueEvent(EVENT_NEXTLED, 0);	// Select next light pattern based on button press
		}
		break;
	case EVENT_REQSLEEP:
		if (BTNSTATE_IDLE != btnState) *(bool*)eventArg = false;	// Disallow sleep while button in use
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}
