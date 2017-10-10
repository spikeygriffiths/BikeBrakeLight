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
#include "UARTmod.h"	// For OSprintf
#include "ACCELmod.h"

bool accelInt = false;

void ACCELWriteReg8(ADXL363_REG reg, U8 val);
U8 ACCELReadReg8(ADXL363_REG reg);
S16 ACCELReadReg16(ADXL363_REG reg);
U8 SPItrx(U8 cData);

ISR(INT2_vect)	// Accelerometer interrupt
{
	accelInt = true;
}

void ACCELEventHandler(U8 eventId, U16 eventArg)
{
	U8 accelState;
	S16 x,y,z;
	
	switch (eventId) {
	case EVENT_INIT:
		// Initialise ATmega SPI
		SPCR = (1<<SPE)|(1<<MSTR)|(0<<SPR0);	// SPI Enable, Master, MSB first, POL & PHA both 0, set clock rate fck/2
		SPSR = (1<<SPI2X);	// Finish selecting fclk/2, being 512KHz at 1MHz sys clk
		break;
	case EVENT_POSTINIT:
		// Configure ADXL363 for generating wake or brake interrupts
		ACCELWriteReg8(ADXL363_THRESH_ACT_L, G / 4);	// > Quarter G on any axis to wake up
		ACCELWriteReg8(ADXL363_TIME_ACT, 10);	// Assume 100Hz sampling rate, so require threshold to be exceeded for 0.1 seconds before interrupting
		ACCELWriteReg8(ADXL363_THRESH_INACT_L, G / 6);	// < 1/6 G on any axis to go to sleep
		ACCELWriteReg8(ADXL363_ACT_INACT_CTL, 0x03);	// Turn on activity and make it relative (to ignore gravity)
		ACCELWriteReg8(ADXL363_INTMAP2, 0x10);	// Map Activity state to INT2
		ACCELWriteReg8(ADXL363_POWER_CTL, 0x0A);	// Set ADXL into Measurement and Wakeup state
		break;
	case EVENT_TICK:
	// Could be EVENT_WAKE?
		if (accelInt) {
			accelInt = false;
			OSprintf("Accel Interrupt\r\n");
			accelState = ACCELReadReg8(ADXL363_STATUS) & 0x70;	// Read whether we're awake (and acknowledge it as well) as well as Active or Inactive
			if (accelState & 0x10) {
				x = ACCELReadReg16(ADXL363_XDATA_L);
				y = ACCELReadReg16(ADXL363_YDATA_L);
				z = ACCELReadReg16(ADXL363_ZDATA_L);
				//OSprintf("ADXL Status register = 0x%2x\r\n", accelState);	
				OSprintf("%d, %d, %d\r\n", x, y, z);	// X = Left / right, Y = Up / Down, Z = Forward / Back
				if (z > (G / 4)) OSIssueEvent(EVENT_BRAKE, 0);	// Quarter G deceleration (is this right?) to cause a BRAKE event
			}
		}
		break;
	case EVENT_BUTTON:
		if (!eventArg) {	// Button release
			OSprintf("ADXL DEVID_AD register = 0x%2x\r\n", ACCELReadReg8(ADXL363_DEVID_AD));	
			//OSprintf("ADXL DevId register = 0x%2x\r\n", ACCELReadReg8(ADXL363_DEVID));	
			//OSprintf("ADXL_Power_Ctl 0x%2x\r\n", ACCELReadReg8(ADXL363_POWER_CTL));
			OSprintf("%d, %d, %d\r\n", ACCELReadReg16(ADXL363_XDATA_L), ACCELReadReg16(ADXL363_YDATA_L), ACCELReadReg16(ADXL363_ZDATA_L));	// X = Left / right, Y = Up / Down, Z = Forward / Back
		}
		break;
	case EVENT_SLEEP:
		SPCR &= ~(1<<SPE);	// Disable SPI when asleep
		break;
	case EVENT_WAKE:
		SPCR |= (1<<SPE);	// Re-enable SPI, assuming still set to master and clock speed
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

void ACCELWriteReg8(ADXL363_REG reg, U8 val)
{
	PORTB &= ~0x01;	// Slave Select low to select ADXL363
	SPCR |= (1<<SPE);// Enable SPI
	SPItrx(0x0A);	// Write register command
	SPItrx(reg);	// Select register
	SPItrx(val);	// Send value
	SPCR &= ~(1<<SPE);// Disable SPI
	PORTB |= 0x01;	// Deselect SS by driving it high at end of transaction
}

U8 ACCELReadReg8(ADXL363_REG reg)
{
	U8 val;
	PORTB &= ~0x01;	// Slave Select low to select ADXL363
	SPCR |= (1<<SPE);// Enable SPI
	SPItrx(0x0B);	// Read register command
	SPItrx(reg);		// Select register
	val = SPItrx(0xFF);	// Send dummy byte to get answer
	SPCR &= ~(1<<SPE);// Disable SPI
	PORTB |= 0x01;	// Deselect SS by driving it high at end of transaction
	return val;
}

S16 ACCELReadReg16(ADXL363_REG reg)
{
	U16 val;
	PORTB &= ~0x01;	// Slave Select low to select ADXL363
	SPCR |= (1<<SPE);// Enable SPI
	SPItrx(0x0B);	// Read register command
	SPItrx(reg);		// Select register
	val = SPItrx(0xFF);	// Send dummy byte to get lo byte
	val |= ((U16)SPItrx(0xFF) << 8);	// Send dummy byte to get hi byte
	SPCR &= ~(1<<SPE);// Disable SPI
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
