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
	EVENT_TICK,
	EVENT_SECOND,
	EVENT_REQSLEEP,
	EVENT_SLEEP,
	EVENT_WAKE,
	EVENT_BUTTON,
	EVENT_BRAKE,
	EVENT_NEXTLED, // Select next LED pattern, either from button tap (NEXTLED_BTN) or time (NEXTLED_TIME)
	EVENT_MOTION,	// True if bike in motion, false if stationary
} Event;

typedef enum { SLEEPTYPE_LIGHT,	/* Allow accelerometer or button to wake us up*/ SLEEPTYPE_DEEP, /* Only button can wake from this */} SleepType;
typedef enum { NEXTLED_BUTTON, NEXTLED_TIME } NextLed;

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
U16 LDRget(void);
U16 TMPget(void);

#endif /* MAIN_H_ */
