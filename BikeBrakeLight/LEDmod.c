/*
 * LEDmod.c
 *
 * Created: 19/09/2017 09:03:37
 *  Author: Spikey
 */ 
#include <avr/io.h>
//#include <stdio.h>
//#include <stdarg.h>
#include <stdbool.h>
#include "main.h"
#include "osstr.h"
#include "UARTmod.h"	// For OSprintf
#include "LEDmod.h"

//static U16 ambientLight;	// Not sure this should be here - could have LDRmod which could sample LDR and advertise it?
static U16 ledLvls[NUMLEDS];	// 16 bits for each LED for accuracy when fading.  Only top 8 bits actually go into PWM hardware
static S16 ledStep[NUMLEDS];
static S16 ledTime;
static U8 ledState;
static U8 ledEffect;
static LED_ROW* ledBackgroundTop;	// Points to a table
static LED_ROW* ledOverride;	// After override finishes, go back to start of background
static LED_ROW* ledRow;	// Points to a row of a table, either override or background, or NULL if no LED activity

const LED_ROW LedOff[] = {
	{{0x0000,0x0000,0x0000},1024,1},	// Slow fade to black, then hold...
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedCirculate[] = {
	{{0x5FFF,0x0000,0x0000},256,256},
	{{0x0000,0x5FFF,0x0000},256,256},
	{{0x0000,0x0000,0x5FFF},256,256},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedPersistent[] = {
	{{0x5FFF,0x0FFF,0x2FFF},64,128},
	{{0x2FFF,0x5FFF,0x0FFF},64,128},
	{{0x0FFF,0x2FFF,0x5FFF},64,128},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedTopBottom[] = {
	{{0x0000,0x0000,0x5FFF},128,512},
	{{0x5FFF,0x5FFF,0x0000},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedTopFlash[] = {
	{{0x5FFF,0x5FFF,0x5FFF},128,512},
	{{0x5FFF,0x5FFF,0x0000},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedBrake[] = {
	{{0xFFFF,0xFFFF,0xFFFF},128,1024},	// 1 second full bright
	{{0x0000,0x0000,0x0000},512,0},	// Fade down
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from this override
};

const LED_ROW* ledEffects[] = {
	LedOff,
	LedCirculate,
	LedPersistent,
	LedTopBottom,
	LedTopFlash,
	NULL	// Terminator
};

// Private function prototypes
void LEDpwm(U16* ledLevels);

void LEDEventHandler(U8 eventId, U16 eventArg)
{
	U8 ledIndex;
	switch (eventId) {
	case EVENT_PREINIT:
		TCCR1A = 0xA9;	// Bits 2-3,4-5 & 6-7 are PWM compare for each main LED, 10=Mark then space, bits 0-1 are WGM0,1
		TCCR1B = 0x09;	// Bits 0-2 select clock (001==No prescaler), bits 3-4 are WGM2,3 (0101 for WGM=Fast PWM, 8 bit)
		break;
	case EVENT_INIT:
		TURNOFF_LED(0);
		TURNOFF_LED(1);
		TURNOFF_LED(2);
		ledEffect = 0;
		LEDBackground((LED_ROW*)NULL);
		break;
	case EVENT_TICK:
		if (ledRow) {
			switch (ledState) {
			case LEDSTATE_PREPAREROW:
				//OSprintf("Start Fading...\r\n");
				ledState = LEDSTATE_FADING;
				// Set up ledSteps for each LED, so we know how much to add for each ms.  Allow for fractional adding
				ledTime = ledRow->fadeMs;
				for (ledIndex = 0; ledIndex < NUMLEDS; ledIndex++) {
					S16 ledDiff = ledRow->targets[ledIndex] - ledLvls[ledIndex];
					ledStep[ledIndex] = ledDiff / ledTime;	// Minimum change in PWM brightness is 1 units / 256 ms, or full scale over a moinute
					//if (0 == ledIndex) OSprintf("Curr: %4x, Trgt: %4x, Diff: %4x, Step: %d\r\n", ledLvls[0], ledRow->targets[0], ledDiff, ledStep[0]);
				}
				break;
			case LEDSTATE_FADING:
				if (ledTime > eventArg) {
					ledTime -= eventArg;
					for (ledIndex = 0; ledIndex < NUMLEDS; ledIndex++) {
						ledLvls[ledIndex] += ledStep[ledIndex];
					}
					LEDpwm(ledLvls);
				} else {
					// Force LEDs to target values directly, to cope with accumulating errors
					for (ledIndex = 0; ledIndex < NUMLEDS; ledIndex++) {
						ledLvls[ledIndex] = ledRow->targets[ledIndex];
					}
					LEDpwm(ledLvls);
					ledTime = ledRow->holdMs;
					//OSprintf("Start Holding...\r\n");
					ledState = LEDSTATE_HOLDING;
				}
				break;
			case LEDSTATE_HOLDING:
				if (ledTime > eventArg) {
					ledTime -= eventArg;
				} else {
					ledRow++;	// Get next row
					if (TABLE_END == ledRow->fadeMs) {	// Check for end of table
						if (LedOff == ledBackgroundTop) {	// Just finished fading down to black, so all LEDs are off
							ledState = LEDSTATE_IDLE;	// Assume all LEDs are now off, because LED_BRAKE also fades down to off before resuming
							//OSprintf("LEDs idle\r\n");
						} else {
							ledRow = ledBackgroundTop;	// Go to top of current background table (maybe NULL)
						}
					}
					if (LEDSTATE_IDLE != ledState) {
						//OSprintf("Prepare next LED row...\r\n");
						ledState = LEDSTATE_PREPAREROW;
					}
				}
				break;
			}
		}
		break;
	case EVENT_BRAKE:
		LEDOverride(LedBrake);
		break;
	case EVENT_REQSLEEP:
		if (LEDSTATE_IDLE != ledState) *(bool*)eventArg = false;	// Disallow sleep
		break;
	case EVENT_BUTTON:
		if (eventArg) {	// Button down
			ledEffect++;
			if (NULL == ledEffects[ledEffect]) ledEffect = 0;	// Wrap at end of table
			LEDBackground((LED_ROW*)ledEffects[ledEffect]);	// Every time we press the button, select next pattern from list
			//ambientLight = LDRget();
			//OSprintf("LDR %d%%\r\n", ambientLight/10);
			IND_LED_ON;
		} else {
			IND_LED_OFF;
		}
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

void LEDBackground(LED_ROW* ledTable)
{
	ledBackgroundTop = ledTable;
	ledOverride = NULL;	// No override by default
	ledRow = ledTable;	// Start at top of table
	ledState = (NULL != ledRow) ? LEDSTATE_PREPAREROW : LEDSTATE_IDLE;
}

void LEDOverride(const LED_ROW* ledTable)
{
	// Leave ledBackgroundTop as it was, so we can restore to it when override is done
	ledOverride = (LED_ROW*)ledTable;
	ledRow = (LED_ROW*)ledTable;	// Start at top of table
	ledState = LEDSTATE_PREPAREROW;
}

void LEDpwm(U16* ledLevels)
{
	if (ledLevels[0]) {
		TURNON_LED(0);	// Enable LED if non-zero light level
		OCR1AL = ledLevels[0] >> 8;	// Put top 8 bits of each LED level into PWM hardware
	} else {
		TURNOFF_LED(0);	// Must turn off LED when zero, since PWM=0 is still a glimmer
	}
	if (ledLevels[1]) {
		TURNON_LED(1);	// Enable LED if non-zero light level
		OCR1BL = ledLevels[1] >> 8;	// Put top 8 bits of each LED level into PWM hardware
	} else {
		TURNOFF_LED(1);	// Must turn off LED when zero, since PWM=0 is still a glimmer
	}
	if (ledLevels[2]) {
		TURNON_LED(2);	// Enable LED if non-zero light level
		OCR1CL = ledLevels[2] >> 8;	// Put top 8 bits of each LED level into PWM hardware
	} else {
		TURNOFF_LED(2);	// Must turn off LED when zero, since PWM=0 is still a glimmer
	}
}
