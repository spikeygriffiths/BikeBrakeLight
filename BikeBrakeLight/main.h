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
#include <avr/pgmspace.h>	// Needed to keep const tables & string, etc. in flash until needed

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef unsigned long long U64;
typedef signed char S8;
typedef signed short S16;
typedef signed long S32;
typedef signed long long S64;

#define OS_BANNER "Spikey Bike Light for ATmega32U4 v0.6"
#define OS_NEWLINE "\r\n"

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
	EVENT_INFO,		// No arg.  Each module can trap this and print something
	EVENT_REQSLEEP,	// Arg is pointer to bool, which is set before issuing event.  To deny sleep, clear the bool
	EVENT_SLEEP,	// Arg is SleepType - see below
	EVENT_WAKE,		// Arg is SleepType - see below
	EVENT_BUTTON,	// Arg is true if button now down
	EVENT_SINGLE_CLICK,	// No arg
	EVENT_DOUBLE_CLICK,	// No arg
	EVENT_LONG_CLICK,	// No arg
	EVENT_BRAKE,	// No arg
	EVENT_USB,		// Arg is true if just connected
	EVENT_NEXTLED,	// Select next LED series
	EVENT_MOTION,	// True if bike in motion, false if stationary for too long
	EVENT_LDR,		// Arg is light level as a percentage
	EVENT_DAYLIGHT,	// Arg is DAYLIGHT_xxx
	EVENT_BATTERY,	// Arg is battery level as a percentage
	EVENT_CHARGING,	// No arg - issued when USB attached
	EVENT_CHARGED,	// No arg - issued once STAT line goes low from battery charging circuit
} Event;

enum {
	DAYTIME_UNKNOWN,
	DAYTIME_NIGHT,
	DAYTIME_DUSK,	// Also DAWN - basically the intermediate state between night and day
	DAYTIME_DAY,
};

#define SYSBITNUM_DAY (0)	// 2 bits for this
#define SYSBITNUM_MOTION (2)
#define SYSBITNUM_USB (3)
#define SYSBITMASK_DAY (3<<SYSBITNUM_DAY)	// Bits 0 & 1 for DAY (See DAYTIME_xxx above for values)
#define SYSBITMASK_MOTION (1<<SYSBITNUM_MOTION)
#define SYSBIT_STATIONARY (0<<SYSBITNUM_MOTION)	// Bit 2 for whether we're in motion or stationary
#define SYSBIT_MOTION (1<<SYSBITNUM_MOTION)
#define SYSBITMASK_USB (1<<SYSBITNUM_USB)
#define SYSBIT_NOUSB (0<<SYSBITNUM_USB)	// Bit 3 for whether USB attached(1) or detached(0)
#define SYSBIT_USB (1<<SYSBITNUM_USB)

typedef enum { SLEEPTYPE_LIGHT,	/* Allow accelerometer or button to wake us up*/ SLEEPTYPE_DEEP, /* Only button can wake from this */} SleepType;

#define NOTANUMBER_U8 0xFF
#define NOTANUMBER_U16 0xFFFF
#define NOTANUMBER_U32 0xFFFFFFFF
#define NOTANUMBER_S8 0x80
#define NOTANUMBER_S16 0x8000
#define NOTANUMBER_S32 0x80000000

#define MS_PERSEC (1000)

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
#define abs(a) ((a < 0) ? -a : a)

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
U8 daylight;	// Used by ADCmod as well as LEDmod
U8 sysBits;

// Public functions
void OSSleep(int sleepType);

#endif /* MAIN_H_ */
