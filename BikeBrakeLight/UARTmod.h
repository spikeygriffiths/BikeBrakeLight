// UARTmod.h

#ifndef UARTMOD_H_
#define UARTMOD_H_
#include "main.h"
#include <stdarg.h>

#ifdef SWITCH_UART
void UARTEventHandler(U8 eventId, U16 eventArg);
void UARTInit(void);
void UARTputc(char c);
bool UARTgetc(char* c);
bool UARTgets(char* buf);
#endif //def SWITCH_UART
void OSprintf(char* str, ...);	// Actually UARTprintf()

#endif	//ndef UARTMOD_H_