/*
 * osstr.c
 *
 * Created: 15/08/2017 23:20:29
 *  Author: Spikey
 */ 
#include "OSstr.h"
#include "main.h"

#include <stdarg.h>

static char* ramPtr;

char* OSitoa(int val, char* buf, int radix, U8 pad) // buf must be 10 or more chars long, radix must be <=36
{
    int wasNegative = false;
    U8 count = 0;
    U8 i;
    unsigned int uval = (unsigned int)val;
    char* pDigit = buf+10; // Move to end of buffer, to work backwards

    if (pad) {
        char padc = (16 == radix) ? '0' : ' ';
        for (i = 0; i < 10; i++) {
            buf[i] = padc;
        }
    }
    *--pDigit = 0; // Terminate string
    if ((10 == radix) && (val < 0)) { // Decimal numbers may be negative
        wasNegative = true;
        uval = -val;
    }
    if (uval) {
        while (uval) {
            char digit = uval%radix;
            *--pDigit = digit + ((digit > 9) ? '7' : '0'); // 7 chars between '9' and 'A' in ASCII
            uval /= radix;
            count++;
        }
    } else {
        *--pDigit = '0'; // Simple case of 0 being passed in
        count = 1;
    }
    if (wasNegative) *--pDigit = '-';
    if (pad) pDigit -= ((pad < count) ? 0 : (pad - count));
    return pDigit; // Returns start of actual string of digits (inside original buffer, but probably not at the start)
}
	
void RAMputc(char c)
{
	*ramPtr++ = c;
}

int OSsprintf(char* buf, char* str, ...)
{
    va_list args;
    va_start(args, str);
    ramPtr = buf;
    OSvsprintf(RAMputc, str, args);
    va_end(args);
    return ramPtr-buf-1;    // Return size of new string (not including terminating '\0')
}

void OSvsprintf(PUTC putc, char* str, va_list args) // Minimal vsprintf(), extensively modified from http://codereview.stackexchange.com/questions/96354/sample-printf-implementation
{
    char ch;  // Character under consideration
    char acTemp[10];  // Buffer to hold string of digits
    char* pcTemp;

    while ((ch = *str++)) {
	    U8 pad = 0;
        if (ch == '%') {
            switch (ch = *str++) {
            case '2': pad = 2; ch = *str++; break;
            case '4': pad = 4; ch = *str++; break;
            case '8': pad = 8; ch = *str++; break;
            }
            switch (ch) {
            case 'd': {
                pcTemp = OSitoa(va_arg(args, int), acTemp, 10, pad);
                while (*pcTemp) putc(*pcTemp++);  // Copy decimal
            } break;
            case 'x': {
                pcTemp = OSitoa(va_arg(args, int), acTemp, 16, max(pad,2));
                while (*pcTemp) putc(*pcTemp++);  // Copy hex
            } break;
            case 's': {
                pcTemp = va_arg(args, char*);
                while (*pcTemp) putc(*pcTemp++);  // Copy string
            } break;
            case 'c': putc(va_arg(args, int)); break; // Copy char
            case '%': putc(ch); break;// Allow for %% to be printed as %
            // default is to absorb the character after the % if we don't understand it
            } // end switch
        } else putc(ch);  // Just copy char directly
    } // end while
    putc('\0');  // Terminate string
}

