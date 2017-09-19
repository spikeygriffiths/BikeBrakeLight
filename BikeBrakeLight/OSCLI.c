/*
 * OSCLI.c
 *
 * Created: 18/09/2017 18:05:50
 *  Author: Spikey
 */

#include "UARTmod.h"
#include "main.h"
#include "OSCLI.h"

char* OSCLI(char* cmd)
{
	OSprintf("Executing %s\r\n", cmd);
	return "Bad cmd";
}
