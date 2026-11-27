#include "ha_units.h"

#include <string.h>

namespace
{
    struct ClassUnits
    {
        const char *device_class;
        const char *units[16];   // список заканчивается nullptr
    };

    // Взято из homeassistant.components.sensor.const.DEVICE_CLASS_UNITS
    const ClassUnits KNOWN[] = {
        {HA_CLASS_ENERGY, {"J", "kJ", "MJ", "GJ", "mWh", "Wh", "kWh", "MWh", "GWh", "TWh",
                           "cal", "kcal", "Mcal", "Gcal", nullptr}},
        {HA_CLASS_WATER, {"L", "gal", "ft³", "m³", "CCF", "MCF", nullptr}},
        {HA_CLASS_GAS, {"L", "ft³", "m³", "CCF", "MCF", nullptr}},
        {HA_CLASS_VOLTAGE, {"V", "mV", "μV", "kV", "MV", nullptr}},
        {HA_CLASS_BATTERY, {"%", nullptr}},
        {HA_CLASS_SIGNAL, {"dB", "dBm", nullptr}},
    };

    bool is_empty(const char *s)
    {
        return s == nullptr || s[0] == 0;
    }
}

bool ha_unit_matches_device_class(const char *device_class, const char *unit)
{
    // Сущность без класса устройства: единица произвольная, HA не проверяет
    if (is_empty(device_class))
        return true;

    for (const ClassUnits &known : KNOWN)
    {
        if (strcmp(known.device_class, device_class) != 0)
            continue;

        for (const char *const *u = known.units; *u != nullptr; ++u)
        {
            if (strcmp(*u, unit == nullptr ? "" : unit) == 0)
                return true;
        }
        return false;
    }

    // Класс без списка единиц (timestamp и подобные): единицы быть не должно
    return is_empty(unit);
}
