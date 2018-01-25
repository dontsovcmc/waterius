#ifndef _POWERSAVE_h
#define _POWERSAVE_h

#include <Arduino.h>

#define ESP_RESET_PIN 1			// Pin number connected to ESP reset pin. This is for waking it up.
#define BUTTON_PIN    4         // + interrupt PCINT4
#define BUTTON2_PIN   3

void gotoDeepSleep( uint32_t seconds, uint32_t *counter, uint32_t *counter2);
void resetWatchdog();

void wakeESP();
//void sensorPower( bool powerOn );

uint16_t readVcc();

#endif

