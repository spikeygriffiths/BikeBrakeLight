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
#define BTN_IGNOREMS (1000)	// After a double-click, ignore button for a while to avoid nervous triple-click

typedef enum {
	BTNSTATE_IDLE,
	BTNSTATE_FIRSTPRESS,
	BTNSTATE_FIRSTRELEASE,
	BTNSTATE_SECONDPRESS,
	BTNSTATE_FIRSTHOLD,	// Held down for a long time
	BTNSTATE_IGNORE,	// Ignore button for a while
	//BTNSTATE_WAITHOLDRELEASE,	// Now just waiting for release so we can issue "LONG_CLICK"
} BtnState;


void BTNEventHandler(Event event, U16 eventArg);

#endif /* BTNMOD_H_ */