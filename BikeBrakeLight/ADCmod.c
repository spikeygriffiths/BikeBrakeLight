/*
 * ADCmod.c
 *
 * Created: 10/10/2017 19:00:11
 *  Author: Spikey
 */ 

#include <avr/io.h>
#include "main.h"
#include "log.h"
#include "UARTmod.h"
#include "LEDmod.h"
#include "ADCmod.h"

static U16 ldrVal;	// In 10 bits, so pretty much percentage with 1 decimal place
static S16 temperature;	// In 10 bits, but not sure how to convert the value into Celsius, so not much use...
static U16 ldrTimerMs;	// Time until next light level sample

void ADCEventHandler(Event event, U16 eventArg)
{
	switch (event) {
	case EVENT_INIT:
		ldrVal = NOTANUMBER_U16;
		temperature = NOTANUMBER_S16;	// In 10 bits, but not sure how to convert the value into Celsius, so not much use...
		ldrTimerMs = 1;	// Get ambient light immediately
		break;
	case EVENT_TICK:
		if (ldrTimerMs) {
			if (ldrTimerMs > eventArg) {
				ldrTimerMs -= eventArg;
			} else {
				ldrVal = LDRget();
				OSIssueEvent(EVENT_LDR, ldrVal);
			}
		}
		break;
	case EVENT_LDR:
	switch (daylight) {
	case DAYTIME_NIGHT:
		if (eventArg > DAY_THRESHOLD + 5) {	// Add some margin for hysteresis
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DAY);
		} else if (eventArg > DARK_THRESHOLD + 5) {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DUSK);
		} else return;	// else hasn't changed enough, so leave as NIGHT
		break;
	case DAYTIME_DUSK:
		if (eventArg < DARK_THRESHOLD) {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_NIGHT);
		} else if (eventArg > DAY_THRESHOLD + 5) {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DAY);
		} else return;	// else hasn't changed enough, so leave as DUSK
		break;
	case DAYTIME_DAY:
		if (eventArg < DARK_THRESHOLD) {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_NIGHT);
		} else if (eventArg < DAY_THRESHOLD) {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DUSK);
		} else return;	// else hasn't changed enough, so leave as DAY
		break;
	default:	// In case we didn't have a previous idea of light level
		if (eventArg < DARK_THRESHOLD) {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_NIGHT);
		} else if (eventArg > DAY_THRESHOLD) {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DAY);
		} else {
			OSIssueEvent(EVENT_DAYLIGHT, DAYTIME_DUSK);	// Neither fully night or day
		}
		break;
	}
	OSprintf("LDR %d\r\n", eventArg);	// If we get here then we have issued a new daylight event
	break;
	case EVENT_REQSLEEP:
		if (NOTANUMBER_U16 == ldrVal) *(bool*)eventArg = false;	// Disallow sleep until we've got a light level
		break;
	case EVENT_WAKE:
		ldrTimerMs = 1;	// Schedule light level whenever we wake up from sleep
		break;
	case EVENT_INFO:
		OSprintf("LDR %d%%%s", ldrVal, OS_NEWLINE);
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

U16 BATTget(void)	// Returns percentage of battery, where 100% is fully-charged
{
	U16 battVal = 0;

	LEDDisable();	// Stop LEDs while measuring battery
	DIDR0 |= BATT_VAL;	// Disable Digital input for ADC6
	ADMUX = 0x06;	// Select ADC for port ADC6, right-justified result (ADLAR=0)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescaler
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish a second time to get a real reading
	battVal = ADCL | (ADCH << 8);	// Sample 10-bit value
	ADCSRA = 0x00;	// Shut down ADC to save power
	DIDR0 &= ~BATT_VAL;	// Re-enable Digital input for ADC6
	return (battVal / 10);	// Convert to percentage, where 100% is full, but 50% is half-nominal voltage.  Might need to adjust this, so that 0% is "About to die"
}

U16 LDRget(void)	// Returns light level as a percentage, where 100% is bright light
{
	U16 lightLvl = 0;
	
	LEDDisable();	// Stop LEDs while measuring ambient light
	DIDR0 |= LDR_VAL;	// Disable Digital input for ADC4
	//DDRF &= ~LDR_EN;	// Enable LDR by making EN an input
	PORTF |= LDR_EN;	// Enable LDR by setting enable line high
	ADMUX = 0x04;	// Select ADC for port ADC4, right-justified result (ADLAR=0)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescaler
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish a second time to get a real reading
	lightLvl = ADCL | (ADCH << 8);	// Sample 10-bit value
	ADCSRA = 0x00;	// Shut down ADC to save power
	DDRF |= LDR_EN;	// Make LDR_EN an output
	PORTF &= ~LDR_EN;	// Disable LDR to save power
	DIDR0 &= ~LDR_VAL;	// Re-enable Digital input for ADC4
	lightLvl = 1024-lightLvl;	// Flip it round, since it's wired to give large readings when dark and low ones when bright
	ldrTimerMs = SAMPLEFREQS_LDR * MS_PERSEC;	// Schedule next background read
	return (lightLvl / 10);	// Convert to percentage
}

S16 TMPget(void)
{
	U16 tempLvl = 0;
	ADMUX = 0xC7;	// Select ADC for Temperature, right-justified result, using internal 2.56V ref
	ADCSRB = 0x20;	// Set bit 5 to select Temperature (MUX is spread across two registers)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	// Poll ADC again to get actual value
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish (using ADSC bit going low)
	tempLvl = ADCL | (ADCH << 8);	// Sample value (must read ADCL first)
	ADCSRA = 0x00;	// Shut down ADC to save power
	return tempLvl;
}
