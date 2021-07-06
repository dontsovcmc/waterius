#include "voltage.h"

void Voltage :: begin() {
    voltage=ESP.getVcc();
    minimum=voltage;
    maximum=voltage;
};

void Voltage :: get() {
    voltage=ESP.getVcc();
    if(voltage<minimum) minimum=voltage;
    if(voltage>maximum) maximum=voltage;
};


uint16_t Voltage :: diff() {
    return maximum-minimum;
};

uint16_t Voltage :: min() {
    return minimum;
};

uint16_t Voltage :: max() {
    return maximum;
};

uint16_t Voltage :: value() {
    return voltage;
};

bool Voltage :: low_voltage() {
    return diff() >= LOW_BATTERY_DIFF_MV;
};


