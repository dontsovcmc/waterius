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

uint16_t alarm_interval_ticks(const bool vacation, const uint8_t counter_type,
                              const uint16_t threshold, const uint16_t factor,
                              const bool electricity)
{
    if (!counts_impulses(counter_type))
    {
        return 0;
    }
    if (vacation)
    {
        return UINT16_MAX;
    }
    if (!factor_configured(factor))
    {
        return 0;
    }
    return flow_to_interval_ticks(threshold, factor, electricity);
}

AlarmInputState alarm_input_state(const uint8_t counter_type, const uint16_t factor,
                                  const uint8_t attiny_version)
{
    if (!counts_impulses(counter_type))
    {
        return ALARM_INPUT_NO_INPUT;
    }
    if (attiny_version < ATTINY_VER_ALARM)
    {
        return ALARM_INPUT_NO_ATTINY;
    }
    if (!factor_configured(factor))
    {
        return ALARM_INPUT_NO_FACTOR;
    }
    return ALARM_INPUT_READY;
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

bool alarm_delivered(const uint8_t mask, const SessionStatus &status)
{
    if (mask == CONFIRM_ANY)
    {
        return status.delivered_any;
    }

    const uint8_t bits[3] = {CONFIRM_WATERIUS, CONFIRM_HTTP, CONFIRM_MQTT};
    const SendStatus sent[3] = {status.waterius, status.http, status.mqtt};

    bool required = false;   // хоть один обязательный получатель настроен
    bool all_ok = true;

    for (uint8_t i = 0; i < 3; i++)
    {
        if (!(mask & bits[i]) || sent[i] == SEND_SKIPPED)
        {
            continue;   // не отмечен или выключен - не в счёт
        }
        required = true;
        if (sent[i] != SEND_OK)
        {
            all_ok = false;
        }
    }

    if (!required)
    {
        return status.delivered_any;   // требовать некого
    }
    return all_ok;
}
