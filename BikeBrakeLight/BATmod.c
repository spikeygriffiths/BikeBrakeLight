/*
 * BATmod.c
 *
 * Created: 30/10/2017 21:18:57
 *  Author: Spikey
 */ 

#include <avr/io.h>
#include "main.h"
#include "log.h"
#include "UARTmod.h"
#include "LEDmod.h"
#include "ADCmod.h"
#include "BATmod.h"

static U16 battTimerMs;	// Time until next battery sample
static U16 battPercentage;
static bool usbAttached;
static bool battCharging;

void BATEventHandler(Event event, U16 eventArg)
{
	switch (event) {
	case EVENT_INIT:
		battPercentage = NOTANUMBER_U16;
		battTimerMs = 1 * MS_PERSEC;
		usbAttached = true;	// Assume if we're starting from power-up, USB must be attached(?)
		battCharging = true;	// Further assume that battery ought to be charging if we're starting, presumably with USB attached(?)
		break;
	case EVENT_TICK:
		if (battTimerMs) {
			if (battTimerMs > eventArg) {
				battTimerMs -= eventArg;
			} else {
				battPercentage = BATTget();
				OSIssueEvent(EVENT_BATTERY, battPercentage);
				battTimerMs = (U16)SAMPLEFREQS_BATT * (U16)MS_PERSEC;
			}
		}
		break;
	case EVENT_SECOND:
		if (usbAttached && battCharging) {
			battCharging = (0 != (PORTE & 0x40));
			if (!battCharging) {
				OSIssueEvent(EVENT_CHARGED, 0);	// We've just stopped charging
			}
		}
		break;
	case EVENT_USB:
		usbAttached = eventArg;
		if (eventArg) {
			OSIssueEvent(EVENT_CHARGING, 0);	// Assume must be charging when USB attached
		} else {
			LEDShowPercentage(battPercentage);		// And then display battery voltage via main LEDs
		}
		break;
	/*case EVENT_DOUBLE_CLICK:
		battTimerMs = 1;	// Schedule battery read very soon
		LEDShowPercentage(battPercentage);		// And then display battery voltage via main LEDs
		break;*/
	case EVENT_REQSLEEP:
		if (NOTANUMBER_U16 == battPercentage) *(bool*)eventArg = false;	// Disallow sleep until we've got a battery reading
		break;
	case EVENT_INFO:
		OSprintf("Batt %d%%%s", battPercentage, OS_NEWLINE);
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

