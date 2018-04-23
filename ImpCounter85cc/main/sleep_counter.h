#ifndef _SLEEP_COUNTER_h
#define _SLEEP_COUNTER_h

#include <Arduino.h>
#include "Setup.h"
#include <EdgeDebounceLite.h>

#define BUTTON_PIN    4 
#ifndef DEBUG
  #define BUTTON2_PIN   3  //т.к. 3-й пин используется для логгирования
#endif


struct Counter {
	uint16_t i;
	bool state;
  
  uint8_t pin;
	EdgeDebounceLite debounce;

	Counter();
	void check();
};

void blink(uint16_t msec);
void gotoDeepSleep( uint16_t minutes, Counter *counter, Counter *counter2);
void resetWatchdog();

#endif

