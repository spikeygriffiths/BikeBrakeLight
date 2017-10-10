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
#include "ADCmod.h"

static U16 ldrVal;	// In 10 bits, so pretty much percentage with 1 decimal place
static U16 battVal;	// In 10 bits, so pretty much percentage with 1 decimal place
static S16 temperature;	// In 10 bits, but not sure how to convert the value into Celsius, so not much use...
static U16 ldrTimerS;	// Time until next light level sample
static U16 battTimerS;	// Time until next battery sample

void ADCEventHandler(Event event, U16 eventArg)
{
	switch (event) {
	case EVENT_INIT:
		ldrVal = NOTANUMBER_U16;
		battVal = NOTANUMBER_U16;
		temperature = NOTANUMBER_S16;	// In 10 bits, but not sure how to convert the value into Celsius, so not much use...
		ldrTimerS = 1;	// Get ambient light very shortly
		battTimerS = 10;
		break;
	case EVENT_POSTINIT:
		ldrVal = LDRget();	// Read light level at start up
		break;
	case EVENT_SECOND:
		if (ldrTimerS) {
			if (ldrTimerS > eventArg) {
				ldrTimerS -= eventArg;
			} else {
				ldrVal = LDRget();
				OSIssueEvent(EVENT_LDR, ldrVal);
				OSprintf("LDR %d%%\r\n", ldrVal/10);
			}
		}
		break;
	case EVENT_DOUBLE_CLICK:
		battVal = BATTget();
		OSIssueEvent(EVENT_BATTERY, battVal);
		OSprintf("Batt %d\r\n", battVal);
		break;
	case EVENT_WAKE:
		ldrVal = LDRget();	// Read light level whenever we wake up from sleep
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

U16 BATTget(void)
{
	U16 battMv = 0;	// Battery in millivolts (mV)

//#define BATT_VAL (1 << 6) // on PORTF
	DIDR0 |= BATT_VAL;	// Disable Digital input for ADC6
	ADMUX = 0x06;	// Select ADC for port ADC6, right-justified result (ADLAR=0)
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescaler
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish
	ADCSRA = 0xC7;	// Enable ADC, using 128 as prescalers
	while (ADCSRA & 0x40) ;	// Wait for conversion to finish a second time to get a real reading
	battMv = ADCL | (ADCH << 8);	// Sample 10-bit value
	ADCSRA = 0x00;	// Shut down ADC to save power
	DIDR0 &= ~BATT_VAL;	// Re-enable Digital input for ADC6
	return battMv;
}

U16 LDRget(void)
{
	U16 lightLvl = 0;
	
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
	ldrTimerS = SAMPLEFREQS_LDR;	// Schedule next background read
	return lightLvl;
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

