/*
 * main.h
 *
 * Created: 26/07/2017 23:07:21
 *  Author: Spikey
 */ 

#ifndef MAIN_H_
#define MAIN_H_
#include <stdbool.h>
#include <stddef.h>

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef unsigned long long U64;
typedef signed char S8;
typedef signed short S16;
typedef signed long S32;
typedef signed long long S64;

typedef void PUTC(char);
typedef int CMD(int, char**);

typedef struct {
	char* cmd;
	char* help;
	CMD* cmdFn;
} CMD_ENTRY;

typedef enum {
	EVENT_PREINIT,
	EVENT_INIT,
	EVENT_POSTINIT,
	EVENT_TICK,		// Arg is elapsed ms since last TICK.  (always 1 at the moment)  Used for LED animation
	EVENT_SECOND,	// No arg.  Could be number of seconds since last SECOND, which makes sense when keeping track of time while sleeping
	EVENT_REQSLEEP,	// Arg is pointer to bool, which is set before issuing event.  To deny sleep, clear the bool
	EVENT_SLEEP,	// Arg is SleepType - see below
	EVENT_WAKE,		// Arg is SleepType - see below
	EVENT_BUTTON,	// Arg is true if button now down
	EVENT_DOUBLE_CLICK,	// No arg
	EVENT_BRAKE,	// No arg
	EVENT_USB,		// Arg is true if just connected
	EVENT_NEXTLED,	// Select next LED series
	EVENT_MOTION,	// True if bike in motion, false if stationary
	EVENT_LDR,		// Arg is light level as 10-bit value
	EVENT_BATTERY,	// Arg is battery level (probably as a percentage)
} Event;

typedef enum { SLEEPTYPE_LIGHT,	/* Allow accelerometer or button to wake us up*/ SLEEPTYPE_DEEP, /* Only button can wake from this */} SleepType;

typedef enum {
	BTNSTATE_IDLE,
	BTNSTATE_FIRSTCLICK,
	BTNSTATE_GAP,
	BTNSTATE_SECONDCLICK,
} BtnState;

#define NOTANUMBER_U8 0xFF
#define NOTANUMBER_U16 0xFFFF
#define NOTANUMBER_U32 0xFFFFFFFF
#define NOTANUMBER_S8 0x80
#define NOTANUMBER_S16 0x8000
#define NOTANUMBER_S32 0x80000000

#define MS_PERSEC (1000)
#define BTN_LONGMS (1000)	// Press and hold for at least a second
#define BTN_CLICKMS (500)

// Switches to enable/disable various code blocks
// Comment out to disable each switch
#define SWITCH_UART	// If this is in, then only one interrupt from accelerometer
#define SWITCH_LOG	// In EEPROM.  Quite slow, but useful for small (1K) logs

#ifndef NULL
#define NULL (void *)0
#endif
#define OSIssueEvent(event, eventArg) _OSIssueEvent(event, (U16)eventArg)

void _OSIssueEvent(Event event, U16 eventArg);
void OSEventHandler(Event event, U16 eventArg);

#define min(a,b) ((a < b) ? a : b)
#define max(a,b) ((a > b) ? a : b)
#define abs(a) ((a < 0) -a : a)

//Pinout
// SPI PB0-3 SPI to accelerometer
// OUT PB5,6,7 High power LEDs
//  IN PD0 Button
//  IN PD2,3 Wake from accelerometer
//  IN PE6 STAT from battery charger
// OUT PF0 Indicator LED (?)
// ADC PF4 Light sensor
// OUT PF5 Light sensor enable
// ADC PF6 Battery voltage level
#define LDR_VAL (1 << 4)	// On portF, or also as ADC4
#define LDR_EN (1 << 5) // on PORTF
#define BATT_VAL (1 << 6) // on PORTF
#define BTN (PIND & 0x01)

// Global Variables
char* lastErr;

// Public functions
void OSSleep(int sleepType);

#endif /* MAIN_H_ */
