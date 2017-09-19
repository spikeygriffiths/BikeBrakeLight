// UARTmod.c

#include <avr/io.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "osstr.h"
#include "UARTmod.h"
#include "OSCLI.h"

#define MAX_CMDLEN (32)
char cmdBuf[MAX_CMDLEN];	// Handy buffer for accumulating commands from serial port

void UARTEventHandler(U8 eventId, U16 eventArg)
{
	switch (eventId) {
	case EVENT_INIT:
		UARTInit();
		*cmdBuf = 0;
		break;
	case EVENT_TICK:
		if (UARTgets(cmdBuf)) {
			if ((lastErr = OSCLI(cmdBuf))) {
				OSprintf(lastErr);
			}
			*cmdBuf = 0;	// Ready for a new command
		}
		break;
	}
}

void UARTInit(void)
{
	UBRR1H = 0;
	UBRR1L = 12;	//3!=19200, 6!=9600, but 12==4800baud with 1MHz clock
	UCSR1B = (1<<RXEN1)|(1<<TXEN1);  // enable uart RX and TX
	UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);  // set 8N1 frame format
}

void UARTputc(char c)
{
	while (!(UCSR1A & (1<<UDRE1))) ;  // wait for Uart Data Register Empty bit to be set
	UDR1 = c;  // send char
}
 
bool UARTgetc(char* c) {
	if (UCSR1A & 0x80) {	//(1<<RXC1)) {
		*c = UDR1;
		PORTF ^= IND_LED;	// Toggle indicator LED
		return true;
	}
	return false;
}

bool UARTgets(char* buf)
{
	char c;
	char* endBuf = buf;
	if (UARTgetc(&c)) {
		while (*endBuf++) ;	// Find end of characters accumulated so far
		switch (c) {
		case 13:	// Only interested in Carriage Returns to end lines
			OSprintf("\r\n");
			return true;	// Got a complete command
		case 8:
			if (endBuf > buf) {
				OSprintf("\08 \08");	// Remove the character from the terminal
				*--endBuf = 0;
			}
			break;
		case 10:	// Ignore linefeeds
			break;
		default:
			if (endBuf - buf < MAX_CMDLEN) {
				UARTputc(c);	// Local echo
				*endBuf++ = c;
				*endBuf = 0;	// Keep accumulated string zero-terminated
			}
			break;
		}
	}
	return false;
}

void OSprintf(char* str, ...)	// Actually UARTprintf()
{
    va_list args;
    va_start(args, str);
    OSvsprintf(UARTputc, str, args);
    va_end(args);
}
