#ifndef _WATERIUS_VOLTAGE_h
#define _WATERIUS_VOLTAGE_h

#include <Arduino.h>

#define LOW_BATTERY_DIFF_MV 50  //надо еще учесть качество замеров (компаратора у attiny)
#define ALERT_POWER_DIFF_MV 100

class Voltage
{
 protected:
	 uint16_t voltage;
	 uint16_t minimum;
	 uint16_t maximum;
 public:
	 void begin();
     void get();
     uint16_t diff();
     uint16_t min();
     uint16_t max();
     uint16_t value();
     bool low_voltage();
};

#endif