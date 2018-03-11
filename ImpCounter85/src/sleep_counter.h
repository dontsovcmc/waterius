#ifndef _SLEEP_COUNTER_h
#define _SLEEP_COUNTER_h

#include <Arduino.h>
#include "Setup.h"

#define BUTTON_PIN    4 
#ifndef DEBUG
  #define BUTTON2_PIN   3  //т.к. 3-й пин используется для логгирования
#endif


#ifdef BUTTON2_PIN
#define BUTTON_INTERRUPT (1 << PCINT4 | 1 << PCINT3)
#else
#define BUTTON_INTERRUPT (1 << PCINT4)
#endif

void gotoDeepSleep( uint16_t minutes, uint16_t *counter, uint16_t *counter2);
void resetWatchdog();

#endif

