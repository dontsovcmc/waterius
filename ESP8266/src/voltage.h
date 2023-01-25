#ifndef _WATERIUS_VOLTAGE_h
#define _WATERIUS_VOLTAGE_h

#include "setup.h"
#include "Logging.h"

#define LOW_BATTERY_DIFF_MV 50 // надо еще учесть качество замеров (компаратора у attiny)
#define ALERT_POWER_DIFF_MV 100
#define BATTERY_LOW_THRESHOLD_MV 2900
#define NUM_PROBES 20


class Voltage
{
private:
    uint16_t _voltage;
    uint16_t _min;
    uint16_t _max;
    uint16_t _values[NUM_PROBES] = {0};
    uint8_t _indx = 0;

public:
    Voltage();
    ~Voltage();
    void begin();
    void update();
    uint16_t value();
    uint16_t average();
    uint16_t diff();
    bool low_voltage();
    uint8_t get_battery_level();
};

#endif