/*
 * osstr.h
 *
 * Created: 15/08/2017 23:23:52
 *  Author: Spikey
 */ 


#ifndef OSSTR_H_
#define OSSTR_H_
#include "main.h"
#include <stdarg.h>

char* OSitoa(int val, char* buf, int radix, U8 pad);
void RAMputc(char c);
void OSprintf(char* str, ...);
int OSsprintf(char* buf, char* str, ...);
void OSvsprintf(PUTC putc, char* str, va_list args);

#endif /* OSSTR_H_ */