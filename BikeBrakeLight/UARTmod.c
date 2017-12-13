// UARTmod.c

#include <avr/io.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "osstr.h"
#include "UARTmod.h"
#include "OSCLI.h"

#ifdef SWITCH_UART
//#define UART_READCHAR

#ifdef UART_READCHAR
#define MAX_CMDLEN (32)
char cmdBuf[MAX_CMDLEN];	// Handy buffer for accumulating commands from serial port
#endif	//def UART_READCHAR

void UARTEventHandler(U8 eventId, U16 eventArg)
{
	switch (eventId) {
	case EVENT_INIT:
		UARTInit();
#ifdef UART_READCHAR
		*cmdBuf = 0;
#endif	//def UART_READCHAR
		break;
#ifdef UART_READCHAR
	case EVENT_TICK:
		if (UARTgets(cmdBuf)) {
			if ((lastErr = OSCLI(cmdBuf))) {
				OSprintf(lastErr);
			}
			*cmdBuf = 0;	// Ready for a new command
		}
		break;
#endif	//def UART_READCHAR
	case EVENT_SLEEP:
		UCSR1B = 0x00;	// Disable UART
		break;
	case EVENT_WAKE:
		PRR1 &= ~0x01;	// Re-enable USART by clearing bit in Power Reduction Register
		UARTInit();	// Must re-enable UART after sleep
		break;
	default:
		break;	// Does nothing, but stops useless warnings from the compiler
	}
}

void UARTInit(void)
{
	UBRR1H = 0;
	UBRR1L = 12;	//3!=19200, 6!=9600, but 12==4800baud with 1MHz clock
#ifdef UART_READCHAR
	UCSR1B = (1<<RXEN1)|(1<<TXEN1);  // Enable both uart TX & Rx
#else	//ifndef UART_READCHAR
	UCSR1B = (1<<TXEN1);  // Just enable uart TX (Leave Rx for ADXL363 Int2)
#endif	//def UART_READCHAR
	UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);  // set 8N1 frame format
}

void UARTputc(char c)
{
	while (!(UCSR1A & (1<<UDRE1))) ;  // wait for Uart Data Register Empty bit to be set
	UDR1 = c;  // send char
}

void OSprintf(char* str, ...)	// Actually UARTprintf()
{
    va_list args;
    va_start(args, str);
    OSvsprintf(UARTputc, str, args);
    va_end(args);
}
 
#ifdef UART_READCHAR
bool UARTgetc(char* c) {
	if (UCSR1A & 0x80) {	//(1<<RXC1)) {
		*c = UDR1;
		//PORTF ^= IND_LED;	// Toggle indicator LED
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
#endif	//def UART_READCHAR
#else //def SWITCH_UART
void OSprintf(char* str, ...)
{
	// Just sink everything - hopefully the optimiser will notice this and remove the calls...
}
#endif //def SWITCH_UART
