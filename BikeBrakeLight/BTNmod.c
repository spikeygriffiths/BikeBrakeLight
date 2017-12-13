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
#include "BTNmod.h"

static bool btnInt;
static bool btnDown;	// true when down
static U16 btnTimerMs;	// Time of button being kept in same state
static BtnState btnState;

ISR(INT0_vect)	// Button edge detected
{
	btnDown = (0 != BTN);	// Read button as true when down
	//if (btnDown) { BTN_LED_ON; } else { BTN_LED_OFF; }	// For debugging
	btnInt = true;
}

void BTNEventHandler(Event event, U16 eventArg)
{
	switch (event) {
	case EVENT_POSTINIT:
		btnInt = false;
		btnState = BTNSTATE_IDLE;
		break;
	case EVENT_TICK:
		if (btnInt) {
			btnInt = false;	// So that we don't continue to trigger
			if (BTNSTATE_IGNORE != btnState) OSIssueEvent(EVENT_BUTTON, btnDown);	// btnDown value set directly from ISR
		}
		if (BTNSTATE_IDLE != btnState) {	// Only think about button if not idle
			btnTimerMs += eventArg;	// Time how long button stays in same state
			switch (btnState) {
			case BTNSTATE_FIRSTPRESS:	// Time when button pressed initially
				if (btnTimerMs > BTN_CLICKMS) {
					btnState = BTNSTATE_FIRSTHOLD;	// No longer measuring Click time, because it's been held down too long, but might not count as long press either yet...
				}
				break;
			case BTNSTATE_FIRSTHOLD:	// Time when button pressed and held
				if (btnTimerMs > BTN_HOLDMS)	{	// Read timer as button is held down
					OSIssueEvent(EVENT_LONG_CLICK, 0);
					btnState = BTNSTATE_IDLE;
				}
				break;
			case BTNSTATE_FIRSTRELEASE:	// Time when Button released after first press...
				if (btnTimerMs > BTN_CLICKMS) {	// Use same length of time for release as for initial press
					OSIssueEvent(EVENT_SINGLE_CLICK, 0);	// Not start of double-click, so must be single click
					btnState = BTNSTATE_IDLE;
				}
				break;
			case BTNSTATE_SECONDPRESS:	// Button pressed soon again after earlier tap
				if (btnTimerMs > BTN_CLICKMS) {	// Use same length of time for release as for initial press
					btnState = BTNSTATE_IDLE;	// If second press is too long, then ignore
				}
				break;
			case BTNSTATE_IGNORE:
				if (btnTimerMs > BTN_IGNOREMS) {
					btnState = BTNSTATE_IDLE;	// Once we've stopped ignoring, go back to Idle
				}
				break;
			default:
				break;	// Does nothing, but stops useless warnings from the compiler
			}
		}
		break;
	case EVENT_BUTTON:
		btnTimerMs = 0;	// Start timer if just pressed or just released
		if (eventArg) { BTN_LED_ON; } else { BTN_LED_OFF; }	// For debugging
		switch (btnState) {
		case BTNSTATE_IDLE:
			btnState = (eventArg) ? BTNSTATE_FIRSTPRESS : BTNSTATE_IDLE;	// If pressed when Idle, advance to FirstPress
			break;
		case BTNSTATE_FIRSTPRESS:
			btnState = (!eventArg) ? BTNSTATE_FIRSTRELEASE : BTNSTATE_IDLE;	// If released during FirstPress, advance to FirstRelease
			break;
		case BTNSTATE_FIRSTRELEASE:
			btnState = (eventArg) ? BTNSTATE_SECONDPRESS : BTNSTATE_IDLE;	// If pressed during FirstRelease, advance to SecondPress
			break;
		case BTNSTATE_SECONDPRESS:
			if (!eventArg) {
				OSIssueEvent(EVENT_DOUBLE_CLICK, 0);	// If released during SecondPress, issue DoubleClick
			}
			btnState = BTNSTATE_IDLE;	// Idle regardless
			break;
		case BTNSTATE_FIRSTHOLD:
			btnState = BTNSTATE_IDLE;	// Released during Hold timing, but before held long enough, so ignore
			break;
		default:
			break;
		}
		break;
	case EVENT_DOUBLE_CLICK:
		btnState = BTNSTATE_IGNORE;	// Ignore clicks immediately after a double click, since that'll be nervous taps
		btnTimerMs = 0;	// Start timer when ignoring
		break;
	case EVENT_REQSLEEP:
		if (BTNSTATE_IDLE != btnState) *(bool*)eventArg = false;	// Disallow sleep while button in use
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}
