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
static U8 nightSeriesIndex;	// Index into series of patterns
static U8 duskSeriesIndex;
static U8 daySeriesIndex;
static U8 ledPatternIndex;	// Index into current LED pattern
static U8 ledPatternCycles;
static U8 daylight;
static const LED_PATTERN* ledPatternTable;	// Current table of rows
static const LED_ROW* ledBackgroundTop;	// Points to a table
static const LED_ROW* ledOverride;	// After override finishes, go back to start of background
static const LED_ROW* ledRow;	// Points to a row of a table, either override or background, or NULL if no LED activity

#define O 0x0000	// Off
#define D 0x01FF	// Dim
#define F 0x0FFF	// Full
#define R 256		// Ramp
#define H 0			// Hold

// --- LED levels for percentage
const LED_ROW LedLevel0[] = {
	// 8hr,   4hr,   12hr (NB anti-clockwise)
	{{O,O,D},R,H},	// Intro animation
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,O},R,H},
	{{D,O,O},R,H},
	{{O,O,O},R,H},	
	{{O,O,O},R,H},	// 0%
	{{O,O,O},0,4096},	// Hold 0%
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from override
};

const LED_ROW LedLevel17[] = {
	// 8hr,   4hr,   12hr (NB anti-clockwise)
	{{O,O,D},R,H},	// Intro animation
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,O},R,H},
	{{D,O,O},R,H},
	{{O,O,O},R,H},	
	{{O,O,O},R,H},	// 0%
	{{O,O,D},R,H},	// 17%
	{{O,O,D},0,4096},	// Hold 17%
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from override
};

const LED_ROW LedLevel33[] = {
	// 8hr,   4hr,   12hr (NB anti-clockwise)
	{{O,O,D},R,H},	// Intro animation
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,O},R,H},
	{{D,O,O},R,H},
	{{O,O,O},R,H},	
	{{O,O,O},R,H},	// 0%
	{{O,O,D},R,H},	// 17%
	{{O,O,F},R,H},	// 33%
	{{O,O,F},0,4096},	// Hold 33%
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from override
};

const LED_ROW LedLevel50[] = {
	// 8hr,   4hr,   12hr (NB anti-clockwise)
	{{O,O,D},R,H},	// Intro animation
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,O},R,H},
	{{D,O,O},R,H},
	{{O,O,O},R,H},	
	{{O,O,O},R,H},	// 0%
	{{O,O,D},R,H},	// 17%
	{{O,O,F},R,H},	// 33%
	{{O,D,F},R,H},	// 50%
	{{O,D,F},0,4096},	// Hold 50%
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from override
};

const LED_ROW LedLevel67[] = {
	// 8hr,   4hr,   12hr (NB anti-clockwise)
	{{O,O,D},R,H},	// Intro animation
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,O},R,H},
	{{D,O,O},R,H},
	{{O,O,O},R,H},	
	{{O,O,O},R,H},	// 0%
	{{O,O,D},R,H},	// 17%
	{{O,O,F},R,H},	// 33%
	{{O,D,F},R,H},	// 50%
	{{O,F,F},R,H},	// 67%
	{{O,F,F},0,4096},	// Hold 67%
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from override
};

const LED_ROW LedLevel84[] = {
	// 8hr,   4hr,   12hr (NB anti-clockwise)
	{{O,O,D},R,H},	// Intro animation
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,O},R,H},
	{{D,O,O},R,H},
	{{O,O,O},R,H},	
	{{O,O,O},R,H},	// 0%
	{{O,O,D},R,H},	// 17%
	{{O,O,F},R,H},	// 33%
	{{O,D,F},R,H},	// 50%
	{{O,F,F},R,H},	// 67%
	{{D,F,F},R,H},	// 84%
	{{D,F,F},0,4096},	// Hold 84%
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from override
};

const LED_ROW LedLevel100[] = {
	// 8hr,   4hr,   12hr (NB anti-clockwise)
	{{O,O,D},R,H},	// Intro animation
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,D},R,H},
	{{O,D,F},R,H},
	{{D,F,O},R,H},
	{{F,O,O},R,H},
	{{D,O,O},R,H},
	{{O,O,O},R,H},	
	{{O,O,O},R,H},	// 0%
	{{O,O,D},R,H},	// 17%
	{{O,O,F},R,H},	// 33%
	{{O,D,F},R,H},	// 50%
	{{O,F,F},R,H},	// 67%
	{{D,F,F},R,H},	// 84%
	{{F,F,F},R,H},	// 100%
	{{F,F,F},4096,0},	// Hold 100%
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from override
};


// LEDs off when idle (eg during daylight)
const LED_ROW LedOff[] = {
	{{0x0000,0x0000,0x0000},1024,1},	// Slow fade to black, then hold...
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

// Leds on steady (eg at night time)
const LED_ROW LedOn[] = {
	{{0x5FFF,0x5FFF,0x5FFF},1024,1},	// Fade up to On, then hold...
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

// --- Single Flash LED while charging
const LED_ROW LedFlashTop[] = {
	{{0x0000,0x0000,0x5FFF},128,512},
	{{0x0000,0x0000,0x0000},128,512},
	{{0,0,0},TABLE_END,0}	// Terminator - go back to top of table for background, or restore background from override
};

// --- LED circles
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
	{{0x8000,0x8000,0x8000},512,0},	// Slow Fade down
	{{0x0000,0x0000,0x0000},1024,0},	// Slow Fade down
	{{0,0,0},TABLE_END,0}	// Terminator - restore background from this override
};

// LED patterns with timeouts to step on to next element

const LED_PATTERN ledPatternCircles[] = {
	{LedCirculate,5},
	{LedPersistent,10},
	{NULL,0}	// Terminator
};

const LED_PATTERN ledPatternSingleFlash[] = {
	{LedTopBottom,2},
	{LedLeftRight,2},
	{LedRightLeft,2},
	{NULL,0}	// Terminator
};

const LED_PATTERN ledPatternDoubleSingle[] = {
	{LedTopFlash,5},
	{LedLeftFlash,5},
	{LedRightFlash,5},
	{NULL,0}	// Terminator
};

const LED_PATTERN ledPatternOff[] = {
	{LedOff,0},	// 0 cycles used to indicate "IDLE"
	{NULL,0}	// Terminator
};

const LED_PATTERN ledPatternOn[] = {
	{LedOn,10},
	{NULL,0}	// Terminator
};

const LED_PATTERN ledPatternFlashTop[] = {
	{LedFlashTop,10},
	{NULL,0}	// Terminator
};

enum {
	LEDSERIES_ON,
	LEDSERIES_CIRCLE,
	LEDSERIES_OFF,
	LEDSERIES_SingleFlash,
	LEDSERIES_DoubleSingle,
	LEDSERIES_NULL,	// Terminator of sequence of series - Do not select!
	LEDSERIES_FLASHTOP,	// Beyond table end - can be selected, but not as part of a sequence of series
};

const LED_PATTERN* ledSeries[] = {
	ledPatternOn,
	ledPatternCircles,
	ledPatternOff,
	ledPatternSingleFlash,
	ledPatternDoubleSingle,
	NULL,	// Terminator
	ledPatternFlashTop,	// Beyond table end - can be selected, but not as part of a sequence of series
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
		nightSeriesIndex = LEDSERIES_ON;	// Steady On
		duskSeriesIndex = LEDSERIES_SingleFlash;	// For single flash
		daySeriesIndex = LEDSERIES_OFF;	// For LEDS off.  Should recover these values from EEPROM to cope with power outages
		daylight = DAYTIME_UNKNOWN;	// Wait for first LDR reading to set this properly
		ledRow = 0;	// No animation until we know the light level
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
					ledStep[ledIndex] = ledDiff / ledTime;	// Minimum change in PWM brightness is 1 units / 256 ms, or full scale over a minute
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
							ledPatternIndex = LEDStartPattern(ledPatternIndex+1);
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
	case EVENT_CHARGING:
		LEDStartSeries(LEDSERIES_FLASHTOP); // Show flashing top LED
		break;
	case EVENT_CHARGED:
		LEDStartSeries(LEDSERIES_OFF);	// All off and back to Idle
		break;
	case EVENT_SINGLE_CLICK:
		OSIssueEvent(EVENT_NEXTLED, 0);	// Select next light pattern based on button press
		break;
	case EVENT_REQSLEEP:
		if (LEDSTATE_IDLE != ledState) *(bool*)eventArg = false;	// Disallow sleep unless we're idle
		break;
	case EVENT_SLEEP:
		LEDDisable();
		break;
	case EVENT_LDR:
		switch (daylight) {
		case DAYTIME_NIGHT:
			if (eventArg > DAY_THRESHOLD + 5) {	// Add some margin for hysteresis
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DAY);
			} else if (eventArg > DARK_THRESHOLD + 5) {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DUSK);
			} // else hasn't changed enough, so leave as NIGHT
			break;
		case DAYTIME_DUSK:
			if (eventArg < DARK_THRESHOLD) {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_NIGHT);
			} else if (eventArg > DAY_THRESHOLD + 5) {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DAY);
			} // else hasn't changed enough, so leave as DUSK
			break;
		case DAYTIME_DAY:
			if (eventArg < DARK_THRESHOLD) {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_NIGHT);
			} else if (eventArg < DAY_THRESHOLD) {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DUSK);
			} // else hasn't changed enough, so leave as DAY
			break;
		default:	// In case we didn't have a previous idea of light level
			if (eventArg < DARK_THRESHOLD) {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_NIGHT);
			} else if (eventArg > DAY_THRESHOLD + 5) {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DAY);
			} else {
				OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DUSK);
			}
			break;
		}
		break;
	case EVENT_DAYLIGHT:
		daylight = eventArg;
		switch (daylight) {
		case DAYTIME_NIGHT:
			LEDStartSeries(nightSeriesIndex);	// Nighttime, so turn on back light
			break;
		case DAYTIME_DUSK:
			LEDStartSeries(duskSeriesIndex);	// Dusk, so start flashing
			break;
		case DAYTIME_DAY:
			LEDStartSeries(daySeriesIndex);	// Daylight, so turn LEDs off in normal use
			break;
		}
		break;
	case EVENT_NEXTLED:	// Select next LED series of patterns
		switch (daylight) {
		case DAYTIME_NIGHT:
			nightSeriesIndex = LEDStartSeries(nightSeriesIndex+1);	// Select next night series
			break;
		case DAYTIME_DUSK:
			duskSeriesIndex = LEDStartSeries(duskSeriesIndex+1);	// Select next dusk series
			break;
		case DAYTIME_DAY:
			daySeriesIndex = LEDStartSeries(daySeriesIndex+1);	// Select next day series
			break;
		}
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

int LEDStartSeries(int series)	// A Series of Patterns
{
	if (NULL == ledSeries[series]) series = 0;	// Restart series
	ledPatternTable = ledSeries[series];
	ledPatternIndex = LEDStartPattern(0);	// Start first pattern in series
	return series;
}

int LEDStartPattern(int pattern)	// A Pattern of Rows, with LED levels, a fade and a hold.  Assumes ledPatternTable set up from LEDStartSeries()
{
	if (NULL == ledPatternTable[pattern].pattern) pattern = 0;	// Restart patternIndex if we fall off the end of the table
	ledBackgroundTop = ledPatternTable[pattern].pattern;
	ledPatternCycles = ledPatternTable[pattern].cycles;
	ledOverride = NULL;	// No override by default
	ledRow = ledBackgroundTop;	// Start at top of table
	ledState = (NULL != ledRow) ? LEDSTATE_PREPAREROW : LEDSTATE_IDLE;
	return pattern;
}

void LEDShowPercentage(int val)
{
	if (val > 92) {
		LEDOverride(LedLevel100);
	} else if ((val <= 92) && (val > 76)) {	// 1/12th either side
		LEDOverride(LedLevel84);	// 5/6
	} else if ((val <= 76) && (val > 58)) {
		LEDOverride(LedLevel67);	// 2/3
	} else if ((val <= 58) && (val > 42)) {
		LEDOverride(LedLevel50);	// 1/2
	} else if ((val <= 42) && (val > 25)) {
		LEDOverride(LedLevel33);	// 1/3
	} else if ((val <= 25) && (val > 8)) {
		LEDOverride(LedLevel17);	// 1/6
	} else {	// Less than or equal to 8
		LEDOverride(LedLevel0);
	}
}

void LEDOverride(const LED_ROW* ledTable)
{
	// Leave ledBackgroundTop as it was, so we can restore to it when override is done
	ledOverride = (LED_ROW*)ledTable;
	ledRow = (LED_ROW*)ledTable;	// Start at top of table
	ledState = LEDSTATE_PREPAREROW;
}

void LEDDisable(void)
{
	TURNOFF_LED(0);	// Disable all LEDs while measuring battery voltage or ambient light, or before deep sleep
	TURNOFF_LED(1);
	TURNOFF_LED(2);
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
