#ifndef _POWERSAVE_h
#define _POWERSAVE_h

#include <Arduino.h>

#define ESP_RESET_PIN 1			// Pin number connected to ESP reset pin. This is for waking it up.
#define SENSOR_POWER_PIN 3

void gotoDeepSleep( uint16_t seconds, uint16_t *counter);
void resetWatchdog();

void wakeESP();
void sensorPower( bool powerOn );

uint16_t readVcc();

#endif

