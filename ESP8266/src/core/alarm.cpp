#include "core/alarm.h"

uint16_t flow_to_interval_ticks(const uint16_t threshold, const uint16_t factor,
                                const bool electricity)
{
    if (threshold == 0 || factor == 0)
    {
        return 0; // тревога выключена
    }

    uint32_t ticks;
    if (electricity)
    {
        ticks = ALARM_TICKS_PER_HOUR * 1000UL / ((uint32_t)threshold * factor);
    }
    else
    {
        ticks = ALARM_TICKS_PER_HOUR * factor / threshold;
    }

    if (ticks > UINT16_MAX)
    {
        // Порог ниже представимого: тревога на любой расход. Честнее, чем
        // молча отбросить настройку пользователя
        return UINT16_MAX;
    }
    if (ticks == 0)
    {
        // Порог выше, чем счётчик способен выдать импульсов: округление вниз
        // дало бы ноль, то есть "выключено", а пользователь просил обратное
        return 1;
    }
    return (uint16_t)ticks;
}

uint8_t alarm_bits(const uint8_t attiny_flags, const uint8_t input,
                   const uint8_t attiny_version)
{
    if (attiny_version < ATTINY_VER_ALARM)
    {
        return 0;
    }

    const uint8_t shift = (input == INPUT0_RED) ? ATTINY_ALARM_SHIFT0 : ATTINY_ALARM_SHIFT1;
    return (attiny_flags >> shift) & ATTINY_ALARM_MASK;
}

bool alarm_configurable(const uint8_t counter_type, const uint16_t factor)
{
    if (counter_type == CounterType::NONE)
    {
        return false;
    }
    if (factor == 0 || factor == AUTO_IMPULSE_FACTOR || factor == AS_COLD_CHANNEL)
    {
        return false;
    }
    return true;
}
