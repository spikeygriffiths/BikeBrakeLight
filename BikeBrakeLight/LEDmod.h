/*
 * LEDmod.h
 *
 * Created: 19/09/2017 09:06:47
 *  Author: Spikey
 */ 

#ifndef LEDMOD_H_
#define LEDMOD_H_

#define IND_LED (1 << 0) // on PORTF
#define IND_LED_ON PORTF |= IND_LED
#define IND_LED_OFF PORTF &= ~IND_LED
#define LED(n) (1 << (5+n))	// on PORTB.  LED(0)=Bottom left, LED(1)=Bottom right, LED(2)=Top
#define TURNON_LED(led) DDRB |= LED(led)	// LEDs numbered 0,1,2
#define TURNOFF_LED(led) DDRB &= ~LED(led)	// LEDs numbered 0,1,2.  Turn off by making the port an input

#define TABLE_END (0x7FFF)	// To mark end of table when used for Fade
#define NUMLEDS (3)	// Numbered 0,1,2

enum {
	LEDSTATE_IDLE,
	LEDSTATE_PREPAREROW,
	LEDSTATE_FADING,
	LEDSTATE_HOLDING,
} _LEDSTATE;
typedef enum _LEDSTATE LEDSTATE;

struct _LED_ROW {
	U16 targets[NUMLEDS];	// 0 = off, FFFF = full brightness
	U16 fadeMs;
	U16 holdMs;
};	// _LED_ROW;
typedef struct _LED_ROW LED_ROW;

// Public functions
void LEDEventHandler(U8 eventId, U16 eventArg);
void LEDBackground(LED_ROW* ledTable);
void LEDOverride(const LED_ROW* ledTable);

#endif /* LEDMOD_H_ */