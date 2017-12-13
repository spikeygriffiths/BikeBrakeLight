/*
 * ACCELmod.c
 *
 * Created: 18/09/2017 20:53:21
 *  Author: Spikey
 */
#include <avr/io.h>
//#include <stdio.h>
//#include <stdarg.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "main.h"
#include "osstr.h"
#include "LEDmod.h"
#include "UARTmod.h"	// For OSprintf
#include "ACCELmod.h"

static bool accelInt;	// Local variable
static ACCELSTATE accelState;
static int accelTimeoutMs;
static int activityCount;

void ACCELWriteReg8(ADXL363_REG reg, U8 val);
U8 ACCELReadReg8(ADXL363_REG reg);
void ACCELWriteReg16(ADXL363_REG reg, U16 val);
S16 ACCELReadReg16(ADXL363_REG reg);
int ACCELGetAxis(ADXL363_REG reg);	// Use ADXL363_XDATA_L for X, ADXL363_YDATA_L for Y and ADXL363_ZDATA_L for Z
U8 SPItrx(U8 cData);
bool onBike;

ISR(INT2_vect)	// Accelerometer interrupt
{
	ACC_LED_ON;	// Turn it off by pressing the button.  Just for debugging
	accelInt = true;
}

void ACCELEventHandler(U8 eventId, U16 eventArg)
{
	S16 x,y,z;

	switch (eventId) {
	case EVENT_INIT:
		accelInt = false;
		accelState = ACCELSTATE_INIT;	// Interrupt only works after acknowledging first IRQ
		accelTimeoutMs = 100;	// Complete initialisation shortly after startup
		// Initialise ATmega SPI
		SPCR = (1<<MSTR)|(0<<SPR0);	// SPI Master, MSB first, POL & PHA both 0, set clock rate fck/2
		SPSR = (1<<SPI2X);	// Finish selecting fclk/2, being 512KHz at 1MHz sys clk
		break;
	case EVENT_POSTINIT:
		// Configure ADXL363 for generating wake or brake interrupts
		ACCELWriteReg16(ADXL363_THRESH_ACT_L, BRAKING_ACCELERATION);
		ACCELWriteReg8(ADXL363_TIME_ACT, 10);	// Use 100Hz sampling rate, so require threshold to be exceeded for 0.1 seconds before interrupting
		ACCELWriteReg16(ADXL363_THRESH_INACT_L, FORWARD_MOTION);
		ACCELWriteReg16(ADXL363_TIME_INACT_L, 10);	// Use 100Hz sampling rate, so require threshold to be exceeded for 0.1 seconds before interrupting
		ACCELWriteReg8(ADXL363_ACT_INACT_CTL, 0x03);	// Turn on activity (but not inactivity) and make them relative (to ignore gravity).  Enable default mode
		ACCELWriteReg8(ADXL363_INTMAP2, 0x10);	// Map Activity (but not Inactivity) state to INT2
		ACCELWriteReg8(ADXL363_FILTER_CTL, 0x03);	// Set ADXL at 100Hz sampling rate
		ACCELWriteReg8(ADXL363_POWER_CTL, 0x0A);	// Set ADXL into Measurement and Wakeup state
		onBike = false;	// Until we know better
		break;
	case EVENT_TICK:
		if (accelInt) {
			/*U8 accelStatus = */ACCELReadReg8(ADXL363_STATUS)/* & 0x70*/;	// Read whether we're awake (and acknowledge it as well) as well as Active or Inactive
			accelInt = false;
			if (accelState != ACCELSTATE_IGNORING) {
				accelState = ACCELSTATE_MONITOR;
				accelTimeoutMs = eventArg;	// So that it'll immediately monitor the accelerometer
			}
		}
		if (accelTimeoutMs > eventArg) {
			accelTimeoutMs -= eventArg;
		} else {
			switch (accelState) {
			case ACCELSTATE_INIT:
				ACCELReadReg8(ADXL363_STATUS);	// Read and acknowledge interrupt to complete initialisation
				accelState =  ACCELSTATE_IDLE;
				break;
			case ACCELSTATE_IGNORING:
				accelState = ACCELSTATE_IDLE;	// Finished ignoring accelerometer
				break;
			case ACCELSTATE_JUSTFIRED:
				ACC_LED_OFF;
				// Fall through to MONITOR...
			case ACCELSTATE_MONITOR:	//  Monitor accelerometer every 100ms when active.  If Z-axis goes too low for motion for too long (60s?) then issue EVENT_MOTION with False (to indicate stationary).  Otherwise, issue EVENT_MOTION with True (to indicate moving)
				x = ACCELGetAxis(ADXL363_XDATA_L);
				y = ACCELGetAxis(ADXL363_YDATA_L);
				z = ACCELGetAxis(ADXL363_ZDATA_L);
				if ((abs(x) < 200) && (y > 900)) {	// Make sure light is right way up (and thus assumed to be on a bike) before declaring a braking, or a motion event
					if (!onBike) OSIssueEvent(EVENT_ONBIKE, true);
					if (z < -BRAKING_ACCELERATION) {
						OSIssueEvent(EVENT_BRAKE, 0);	// Less than deceleration (is this right?) to cause a BRAKE event
						accelState = ACCELSTATE_JUSTFIRED;	// Turn LED off next time
					} else accelState = ACCELSTATE_MONITOR;	// Continue monitoring
					if (abs(z) > FORWARD_MOTION) {
						if (0 == activityCount) OSIssueEvent(EVENT_MOTION, true);	// If we were stationary previously
						activityCount = ACTIVITY_TIMEOUT_S * 10;	// We monitor accelerometer 10 times a second, hence the *10
					} else {
						if (activityCount) {	// Count down activity timeout if not moving
							if (0 == --activityCount) {
								OSIssueEvent(EVENT_MOTION, false);	// Finally timed out waiting for movement, so admit that we're stationary
							}
						}
					}
				} else {	// Not on a bike}
					if (onBike) OSIssueEvent(EVENT_ONBIKE, false);
				}
				accelTimeoutMs = 100;	// Check motion again in 100ms
				break;
			case ACCELSTATE_IDLE:
				accelTimeoutMs = 0;
				break;
			} // end switch()
		}
		break;
	case EVENT_ONBIKE:
		onBike = (bool)eventArg;
		break;
	case EVENT_BUTTON:
		accelState = ACCELSTATE_IGNORING;
		accelTimeoutMs = 2 * MS_PERSEC;	// Disable Accelerometer events while pressing button and for a short while afterwards
		break;
	case EVENT_SINGLE_CLICK:
		//OSprintf("PortD = 0x%2x\r\n", PORTD);
		//OSprintf("ADXL AD = 0x%2x\r\n", ACCELReadReg8(ADXL363_DEVID_AD));
		//OSprintf("ADXL Thresh_Act = %d\r\n", ACCELReadReg16(ADXL363_THRESH_ACT_L));
		//OSprintf("ADXL DevId register = 0x%2x\r\n", ACCELReadReg8(ADXL363_DEVID));
		//OSprintf("ADXL_Power_Ctl 0x%2x\r\n", ACCELReadReg8(ADXL363_POWER_CTL));
		//OSprintf("%d, %d, %d\r\n", ACCELReadReg16(ADXL363_XDATA_L), ACCELReadReg16(ADXL363_YDATA_L), ACCELReadReg16(ADXL363_ZDATA_L));	// X = Left / right, Y = Up / Down, Z = Forward / Back
		break;
	case EVENT_REQSLEEP:
		if (ACCELSTATE_IDLE != accelState) *(bool*)eventArg = false;	// Disallow sleep unless we're idle
		break;
	case EVENT_WAKE:
		accelState =  ACCELSTATE_IDLE;	// Ready to handle interrupt
		accelTimeoutMs = 0;
		break;
	case EVENT_INFO: {
		S16 x,y,z;
		x = ACCELGetAxis(ADXL363_XDATA_L);
		y = ACCELGetAxis(ADXL363_YDATA_L);
		z = ACCELGetAxis(ADXL363_ZDATA_L);
		OSprintf("x = %d\r\n", x);	// X = Left(+) / right, Y = Up(+) / Down, Z = Forward(+) / Back
		OSprintf("y = %d\r\n", y);	// X = Left / right, Y = Up / Down, Z = Forward / Back
		OSprintf("z = %d\r\n", z);	// X = Left / right, Y = Up / Down, Z = Forward / Back
		OSprintf("Onbike %d%s", onBike, OS_NEWLINE);
		} break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

void ACCELWriteReg8(ADXL363_REG reg, U8 val)
{
	PORTB &= ~0x01;	// Slave Select low to select ADXL363
	SPIEnable();
	SPItrx(0x0A);	// Write register command
	SPItrx(reg);	// Select register
	SPItrx(val);	// Send value
	SPIDisable();
	PORTB |= 0x01;	// Deselect SS by driving it high at end of transaction
}

U8 ACCELReadReg8(ADXL363_REG reg)
{
	U8 val;
	PORTB &= ~0x01;	// Slave Select low to select ADXL363
	SPIEnable();
	SPItrx(0x0B);	// Read register command
	SPItrx(reg);		// Select register
	val = SPItrx(0xFF);	// Send dummy byte to get answer
	SPIDisable();
	PORTB |= 0x01;	// Deselect SS by driving it high at end of transaction
	return val;
}

void ACCELWriteReg16(ADXL363_REG reg, U16 val)
{
	PORTB &= ~0x01;	// Slave Select low to select ADXL363
	SPIEnable();
	SPItrx(0x0A);	// Write register command
	SPItrx(reg);		// Select register
	SPItrx(val & 0xFF);	// Send lo byte
	SPItrx(val >> 8);	// Send hi byte
	SPIDisable();
	PORTB |= 0x01;	// Deselect SS by driving it high at end of transaction
}

S16 ACCELReadReg16(ADXL363_REG reg)
{
	U16 val;
	PORTB &= ~0x01;	// Slave Select low to select ADXL363
	SPIEnable();
	SPItrx(0x0B);	// Read register command
	SPItrx(reg);		// Select register
	val = SPItrx(0xFF);	// Send dummy byte to get lo byte
	val |= ((U16)SPItrx(0xFF) << 8);	// Send dummy byte to get hi byte
	SPIDisable();
	PORTB |= 0x01;	// Deselect SS by driving it high at end of transaction
	return val;
}

U8 SPItrx(U8 cData)
{
	SPDR = cData;	// Start transmission
	while(!(SPSR & (1<<SPIF))) ;	// Wait for transmission complete
	return SPDR;	// Return Data Register
}

int ACCELGetAxis(ADXL363_REG reg)	// Use ADXL363_XDATA_L for X, ADXL363_YDATA_L for Y and ADXL363_ZDATA_L for Z
{
	return ACCELReadReg16(reg);	// X = Left / right, Y = Up / Down, Z = Forward / Back
}
