/*
 * BTNmod.h
 *
 * Created: 09/10/2017 17:32:54
 *  Author: Spikey
 */ 


#ifndef BTNMOD_H_
#define BTNMOD_H_

#define BTN_HOLDMS (1000)	// Press and hold for at least a second to count as long press
#define BTN_CLICKMS (500)	// Change between press and release within this time to move on

typedef enum {
	BTNSTATE_IDLE,
	BTNSTATE_FIRSTPRESS,
	BTNSTATE_FIRSTRELEASE,
	BTNSTATE_SECONDPRESS,
	BTNSTATE_FIRSTHOLD,	// Held down for a long time
} BtnState;


void BTNEventHandler(Event event, U16 eventArg);

#endif /* BTNMOD_H_ */