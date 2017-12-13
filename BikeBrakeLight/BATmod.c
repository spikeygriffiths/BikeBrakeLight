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

// Private function prototypes
U8 BATTGetLevel(U16 raw);

static U16 battTimerMs;	// Time until next battery sample
static U16 battRaw;
static U16 battPercentage;
static bool usbAttached;
static bool battCharging;
static bool battGood;

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
				battRaw = ADCGetBatt();
				battPercentage = BATTGetLevel(battRaw);
				OSIssueEvent(EVENT_BATTERY, battPercentage);
				battTimerMs = (U16)SAMPLEFREQS_BATT * (U16)MS_PERSEC;
			}
		}
		if (usbAttached) {
			bool newCharging = (0 != (PORTE & 0x40));
			if (newCharging != battCharging) {
				OSIssueEvent(EVENT_CHARGING, newCharging);	// We've just changed between charging and not charging
			}
		}
		break;
	case EVENT_CHARGING:
		if (eventArg) CHG_LED_ON; else CHG_LED_OFF;	// Indicator LED shows Charging state
		battCharging = eventArg;
		break;
	case EVENT_BATTERY:
		if ((eventArg < 10) && battGood) {
			OSIssueEvent(EVENT_BATTGOOD, false);
		} else if (!battGood && (eventArg > 15)) {	// Note anti-hysteresis, using <10% for bad and >15% for good
			OSIssueEvent(EVENT_BATTGOOD, true);
		}
		break;
	case EVENT_BATTGOOD:
		battGood = eventArg;
		break;
	case EVENT_USB:
		usbAttached = eventArg;
		if (!eventArg) {
			OSIssueEvent(EVENT_CHARGING, false);	// Can't be charging if USB removed
			LEDShowPercentage(battPercentage);		// And then display battery voltage via main LEDs
		}
		break;
	case EVENT_DOUBLE_CLICK:
		if (NOTANUMBER_U16 != battPercentage) {
			LEDShowPercentage(battPercentage);		// Display battery voltage via main LEDs
		}
		break;
	case EVENT_REQSLEEP:
		if (NOTANUMBER_U16 == battPercentage) *(bool*)eventArg = false;	// Disallow sleep until we've got a battery reading
		break;
	case EVENT_INFO:
		OSprintf("Batt %d%% (raw %d)%s", battPercentage, battRaw, OS_NEWLINE);
		OSprintf("Charging %d%s", battCharging, OS_NEWLINE);
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

#define BATT_MIN_RAW (400)	// Made-up number - refine this (somehow?)
#define BATT_MAX_RAW (665)	// 669 seen on device with fully-charged battery.  Also seen 697 when LEDs off
#define BATT_RAW_RANGE (BATT_MAX_RAW - BATT_MIN_RAW)

U8 BATTGetLevel(U16 raw)		// Raw is 0-1023 from ADC.  Convert to percentage, where 100% is full, and 50% is half-nominal voltage
{
	if (raw <= BATT_MIN_RAW) {
		return 0;
	} else if (raw >= BATT_MAX_RAW) {
		return 100;
	} else {	// Is inside normal range
		return (U8)(((raw - BATT_MIN_RAW) * 100) / BATT_RAW_RANGE);
	}
}
