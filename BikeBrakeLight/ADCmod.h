/*
 * ADCmod.h
 *
 * Created: 10/10/2017 19:10:02
 *  Author: Spikey
 */ 


#ifndef ADCMOD_H_
#define ADCMOD_H_

#define SAMPLEFREQS_LDR (30)	 // Background rate to sample ambient light

void ADCEventHandler(Event event, U16 eventArg);

U16 ADCGetBatt(void);
U16 LDRget(void);
S16 TMPget(void);

#endif /* ADCMOD_H_ */