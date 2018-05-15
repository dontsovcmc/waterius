#ifndef _SLEEP_COUNTER_h
#define _SLEEP_COUNTER_h

#include <Arduino.h>
#include "Setup.h"
#include "Power.h"

#define BUTTON_PIN    4 
#define BUTTON2_PIN   3  //3-й пин используется для логгирования


#define OPENED       0  //разомкнут
#define FIRST_CHECK  1  //1й раз прочитал замыкание
#define CLOSED       2  //2й раз замыкание - значит точно замкнут

struct Counter 
{
	uint16_t i;

	uint8_t state;  
  
  uint8_t pin;

	Counter(const uint8_t);
	void check();
};


void blink(uint16_t msec);
void gotoDeepSleep(int minutes, Counter *counter, Counter *counter2, ESPPowerButton *esp);
void resetWatchdog();

#endif

