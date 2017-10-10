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

static U16 ledLvls[NUMLEDS];	// 16 bits for each LED for accuracy when fading.  Only top 8 bits actually go into PWM hardware
static S16 ledStep[NUMLEDS];
static S16 ledTime;
static U8 ledState;
static U8 ledSeriesIndex;	// Index into series of patterns
static U8 ledPatternIndex;	// Index into current LED pattern
static U8 ledPatternCycles;
static LED_PATTERN* ledPatternTable;	// Current table of rows
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

// --- Double Single Block
const LED_ROW LedTopBottom[] = {
	{{0x0000,0x0000,0x5FFF},128,512},
	{{0x5FFF,0x5FFF,0x0000},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedLeftRight[] = {
	{{0x0000,0x5FFF,0x0000},128,512},
	{{0x5FFF,0x0000,0x5FFF},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedRightLeft[] = {
	{{0x5FFF,0x0000,0x0000},128,512},
	{{0x0000,0x5FFF,0x5FFF},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};
// --- End Double Single Block

// --- Single Flash Block
const LED_ROW LedTopFlash[] = {
	{{0x5FFF,0x5FFF,0x5FFF},128,512},
	{{0x5FFF,0x5FFF,0x0000},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedLeftFlash[] = {
	{{0x5FFF,0x5FFF,0x5FFF},128,512},
	{{0x5FFF,0x0000,0x5FFF},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

const LED_ROW LedRightFlash[] = {
	{{0x5FFF,0x5FFF,0x5FFF},128,512},
	{{0x0000,0x5FFF,0x5FFF},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};
// --- End Single Flash Block

const LED_ROW LedBrake[] = {
	{{0xFFFF,0xFFFF,0xFFFF},128,1536},	// Rapid ramp up to full bright, then 1.5 second hold
	{{0x0000,0x0000,0x0000},1024,0},	// Slow Fade down
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from this override
};

// LED patterns with timeouts to step on to next element

const LED_PATTERN ledCircle[] = {
	{LedCirculate,5},
	{LedPersistent,10},
	{NULL,0}	// Terminator
};

const LED_PATTERN ledSingleFlash[] = {
	{LedTopBottom,2},
	{LedLeftRight,2},
	{LedRightLeft,2},
	{NULL,0}	// Terminator
};

const LED_PATTERN ledDoubleSingle[] = {
	{LedTopFlash,5},
	{LedLeftFlash,5},
	{LedRightFlash,5},
	{NULL,0}	// Terminator
};

const LED_PATTERN ledsOff[] = {
	{LedOff,0},	// 0 cycles used to indicate "IDLE"
	{NULL,0}	// Terminator
};

const LED_PATTERN* ledSeries[] = {
	ledCircle,
	ledSingleFlash,
	ledDoubleSingle,
	ledsOff,
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
		ledSeriesIndex = 0;	// Select first LED series at start
		LEDStartSeries();
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
						if (0 == ledPatternCycles) {
							ledState = LEDSTATE_IDLE;	// Assume all LEDs are now off, because LED_BRAKE also fades down to off before resuming
							//OSprintf("LEDs idle\r\n");
						} else if (0 == --ledPatternCycles) {
							ledPatternIndex++;
							if (NULL == ledPatternTable[ledPatternIndex].pattern) ledPatternIndex = 0;	// Restart patternIndex if we fall off the end of the table
							LEDStartPattern();
						}
						ledRow = ledBackgroundTop;	// Go to top of current background table (maybe NULL)
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
	case EVENT_USB:
		if (eventArg) {
			// Start charging, and change LED behaviour to show battery charge state
		} else {
			// Show battery voltage via LEDs until button press or for 2 seconds
		}
		break;
	case EVENT_REQSLEEP:
		if (LEDSTATE_IDLE != ledState) *(bool*)eventArg = false;	// Disallow sleep unless we're idle
		break;
	case EVENT_NEXTLED:	// Select next LED series of patterns
		ledSeriesIndex++;
		LEDStartSeries();	// Every time we press the button, select next series of patterns from list
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

void LEDStartSeries(void)	// A Series of Patterns.  Assumes ledSeriesIndex is set up
{
	if (NULL == ledSeries[ledSeriesIndex]) {
		ledSeriesIndex = 0;	// Restart series
	}
	ledPatternTable = ledSeries[ledSeriesIndex];
	ledPatternIndex = 0;	// Start first pattern in current series
	LEDStartPattern();
}

void LEDStartPattern(void)	// A Pattern of Rows, with LED levels, a fade and a hold.  Assumes  ledPatternTable and ledPatternIndex set up
{
	ledBackgroundTop = ledPatternTable[ledPatternIndex].pattern;
	ledPatternCycles = ledPatternTable[ledPatternIndex].cycles;
	ledOverride = NULL;	// No override by default
	ledRow = ledBackgroundTop;	// Start at top of table
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
