/*
 * BATmod.h
 *
 * Created: 30/10/2017 21:23:37
 *  Author: Spikey
 */ 


#ifndef BATMOD_H_
#define BATMOD_H_


#define SAMPLEFREQS_BATT (60)	 // Background rate to sample battery level

void BATEventHandler(Event event, U16 eventArg);


#endif /* BATMOD_H_ */