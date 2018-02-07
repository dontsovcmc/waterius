#ifndef _POWERSAVE_h
#define _POWERSAVE_h

#include <Arduino.h>

#define BUTTON_PIN    0 
#define BUTTON2_PIN   2

#ifdef BUTTON2_PIN
#define BUTTON_INTERRUPT (1 << PCINT0 | 1 << PCINT2)
#else
#define BUTTON_INTERRUPT (1 << PCINT0)
#endif

/*
  - считает только в функции
  - не будет выходить из нее, если раз в секунду будут импульсы. но во сне нельзя считать millis() 
  - 
*/

void gotoDeepSleep( uint32_t seconds, uint32_t *counter, uint32_t *counter2);
void resetWatchdog();

#endif

