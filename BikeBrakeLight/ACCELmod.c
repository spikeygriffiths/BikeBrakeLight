/*
 * ACCELmod.c
 *
 * Created: 18/09/2017 20:53:21
 *  Author: Spikey
 */ 
#include <avr/io.h>
//#include <stdio.h>
//#include <stdarg.h>
#include <stdbool.h>
#include "main.h"
#include "osstr.h"
#include "UARTmod.h"	// For OSprintf

U8 ACCELReadReg(U8 reg);
U8 SPItrx(U8 cData);

void ACCELEventHandler(U8 eventId, U16 eventArg)
{
	switch (eventId) {
	case EVENT_INIT:
		// Initialise ATmega SPI
		SPCR = (1<<MSTR)|(0<<SPR0);	// Master, MSB first, POL & PHA both 0, set clock rate fck/2
		SPSR = (1<<SPI2X);	// Finish selecting fclk/2, being 512KHz at 1MHz sys clk
		// Configure ADXL363 for generating wake or brake interrupts
		break;
	case EVENT_BUTTON:
		if (!eventArg) {	// Button release
			OSprintf("ADXL DEVID_AD register = 0x%2x\r\n", ACCELReadReg(0x00));	
			OSprintf("ADXL DEVID_DevId register = 0x%2x\r\n", ACCELReadReg(0x02));	
		}
		break;
	}
}

U8 ACCELReadReg(U8 reg)
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

U8 SPItrx(U8 cData)
{
	SPDR = cData;	// Start transmission
	while(!(SPSR & (1<<SPIF))) ;	// Wait for transmission complete
	return SPDR;	// Return Data Register
}

int ACCELgetZ(void)
{
	return -1;	// Replace this with actual reading!
}
