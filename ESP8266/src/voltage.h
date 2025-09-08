#ifndef _WATERIUS_VOLTAGE_h
#define _WATERIUS_VOLTAGE_h

#include "setup.h"
#include "Logging.h"

#define LOW_BATTERY_DIFF_MV 50 // надо еще учесть качество замеров (компаратора у ESP)
#define ALERT_POWER_DIFF_MV 100
#define BATTERY_LOW_THRESHOLD_MV 2900
#define MAX_PROBES 20

class Voltage
{
private:
    uint16_t _voltage;
    uint16_t _min_voltage;
    uint16_t _max_voltage;
    uint8_t _num_probes;
    uint16_t _probes[MAX_PROBES];

public:
    Voltage();
    ~Voltage() = default;
    void begin();
    void update();
    uint16_t value();
    uint16_t average();
    uint16_t diff();
    bool low_voltage();
    uint8_t get_battery_level();
};

extern Voltage *get_voltage();

#endif