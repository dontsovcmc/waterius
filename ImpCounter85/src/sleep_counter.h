#ifndef _SLEEP_COUNTER_h
#define _SLEEP_COUNTER_h

#include <Arduino.h>

#define BUTTON_PIN    4 
#define BUTTON2_PIN   3


#ifdef BUTTON2_PIN
#define BUTTON_INTERRUPT (1 << PCINT4 | 1 << PCINT3)
#else
#define BUTTON_INTERRUPT (1 << PCINT4)
#endif

/*
  - считает только в функции
  - не будет выходить из нее, если раз в секунду будут импульсы. но во сне нельзя считать millis() 
  - 
*/

void gotoDeepSleep( uint16_t seconds, uint16_t *counter, uint16_t *counter2);
void resetWatchdog();

#endif

