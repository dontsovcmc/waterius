#ifndef _WATERIUS_VOLTAGE_h
#define _WATERIUS_VOLTAGE_h

#include "setup.h"
#include "Logging.h"

#define LOW_BATTERY_DIFF_MV 50 //надо еще учесть качество замеров (компаратора у attiny)
#define ALERT_POWER_DIFF_MV 100

class Voltage
{
protected:
    uint16_t voltage;
    uint16_t minimum;
    uint16_t maximum;

public:
    Voltage();
    ~Voltage();
    void begin();
    void update();
    uint16_t value();
    uint16_t diff();
    bool low_voltage();
};

#endif