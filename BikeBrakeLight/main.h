/*
 * main.h
 *
 * Created: 26/07/2017 23:07:21
 *  Author: Spikey
 */ 

#ifndef MAIN_H_
#define MAIN_H_
#include <stdbool.h>

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
	EVENT_SLEEP,
	EVENT_WAKE,
	EVENT_BUTTON,
} Event;

#define OSIssueEvent(event, eventArg) _OSIssueEvent(event, (U32)eventArg)

void _OSIssueEvent(Event event, U32 eventArg);
void OSEventHandler(Event event, U32 eventArg);

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
#define LED0 (1 << 5)	// on PORTB
#define LED1 (1 << 6)	// on PORTB
#define LED2 (1 << 7)	// on PORTB
#define IND_LED (1 << 0) // on PORTF
#define LGHTLVL_EN (1 << 5) // on PORTF
#define BTN (PIND & 0x01)

// Global Variables
char* lastErr;

// Public functions
U16 LDRget(void);
U16 TMPget(void);


#endif /* MAIN_H_ */
