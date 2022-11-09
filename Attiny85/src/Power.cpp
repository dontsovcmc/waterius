
#include "Power.h"
#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "Setup.h"

ESPPowerPin::ESPPowerPin(const uint8_t p)
    : power_pin(p), wake_up_timestamp(0)
{
    pinMode(power_pin, INPUT);
}

void ESPPowerPin::power(const bool on)
{
    if (on)
    {
        pinMode(power_pin, OUTPUT);

        digitalWrite(power_pin, HIGH);
        wake_up_timestamp = millis();
    }
    else
    {
        digitalWrite(power_pin, LOW);

        pinMode(power_pin, INPUT); // снижаем потребление
        wake_up_timestamp = 0;
    }
}

bool ESPPowerPin::elapsed(const unsigned long msec)
{
    return millis() - wake_up_timestamp > msec;
}
