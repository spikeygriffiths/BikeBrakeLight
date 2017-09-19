// log.c

#include <avr/io.h>
#include <avr/wdt.h>	// Watchdog timer
#include <stdarg.h>
#include "osstr.h"
#include "main.h"
#include "log.h"

// Local variables
static U16 logIndex;
// Local prototypes
void EEPROMwrite(U16 addr, U8 data);

void LOGEventHandler(Event event, U32 eventArg)
{
	switch (event) {
	case EVENT_INIT:
		for (logIndex = 0; logIndex < 0x3FF; logIndex++) {
			EEPROMwrite(logIndex, 0xFF);	// Clear the old log
		}
		logIndex = 0;	// Ready to start logging from start
		break;
	}
}

void LOGprintf(char* str, ...)
{
    va_list args;
    va_start(args, str);
    OSvsprintf(LOGputc, str, args);
    va_end(args);
}

void LOGputc(char item)
{
	EEPROMwrite(logIndex++, (U8)item);
}

void EEPROMwrite(U16 addr, U8 data)
{
	wdt_reset();
	while(EECR & (1<<EEPE)) ;	/* Wait for completion of previous write */
	EEAR = addr;	/* Set up address and Data Registers */
	if (0xFF == data) {	// Erase only
		EECR = (1<<EEMPE) | (1<<EEPM0);	// Write logical one to EEMPE as well as 0 to EEPE and 01 to EEPM0 & 1
	} else { // Erase and program
		EEDR = data;
		EECR = (1<<EEMPE);	// Write logical one to EEMPE as well as 0 to EEPE and 00 to EEPM0 & 1
	}
	EECR |= (1<<EEPE);	// Start eeprom write by setting EEPE (Will auto-clear)
}