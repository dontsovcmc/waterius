#ifndef _SLEEP_COUNTER_h
#define _SLEEP_COUNTER_h

#include <Arduino.h>
#include "Setup.h"

#define BUTTON_PIN    4 
#ifndef DEBUG
  #define BUTTON2_PIN   3  //т.к. 3-й пин используется для логгирования
#endif


#define OPENED       0  //разомкнут
#define FIRST_CHECK  1  //1й раз прочитал замыкание
#define CLOSED       2  //2й раз замыкание - значит точно замкнут

struct Counter {
	uint16_t i;

	uint8_t state;  
  
  uint8_t pin;

	Counter();
	void check();
};

void blink(uint16_t msec);
void gotoDeepSleep( uint16_t minutes, Counter *counter, Counter *counter2);
void resetWatchdog();

#endif

