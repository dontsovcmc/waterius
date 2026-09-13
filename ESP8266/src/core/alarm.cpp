#include "core/alarm.h"

uint16_t volume_to_pulses(const uint16_t litres, const uint16_t factor)
{
    if (litres == 0 || factor == 0)
    {
        return 0; // тревога выключена
    }

    const uint16_t pulses = litres / factor;

    // Порог меньше одного импульса: округление вниз дало бы ноль, то есть
    // "выключено", а пользователь просил обратное
    return pulses ? pulses : 1;
}

uint16_t rate_to_quantum_ticks(const uint16_t rate, const uint16_t factor)
{
    if (rate == 0 || factor == 0)
    {
        return 0; // тревога выключена
    }

    const uint32_t ticks = ALARM_TICKS_PER_HOUR * factor / rate;

    if (ticks > UINT16_MAX)
    {
        // Порог ниже представимого. Портал такое не пропускает
        // (alarm_min_rate), здесь - страховка от чужой настройки
        return UINT16_MAX;
    }
    return ticks ? (uint16_t)ticks : 1;
}

uint16_t hours_to_quanta(const uint16_t hours, const uint16_t rate,
                         const uint16_t factor)
{
    if (hours == 0 || rate == 0 || factor == 0)
    {
        return 0; // тревога выключена
    }

    // квантов = часы / длина кванта в часах = часы * Q / вес
    const uint32_t quanta = (uint32_t)hours * rate / factor;

    if (quanta > UINT16_MAX)
    {
        return UINT16_MAX;
    }
    return quanta ? (uint16_t)quanta : 1;
}

uint16_t alarm_min_rate(const uint16_t factor)
{
    if (factor == 0)
    {
        return 0;
    }

    // наименьшее Q, при котором 14400 * вес / Q ещё влезает в uint16
    const uint32_t q = (ALARM_TICKS_PER_HOUR * factor + UINT16_MAX - 1) / UINT16_MAX;
    return q ? (uint16_t)q : 1;
}

uint16_t alarm_min_hours(const uint16_t rate, const uint16_t factor)
{
    if (rate == 0 || factor == 0)
    {
        return 0;
    }

    // наименьшее T, при котором квантов набирается хотя бы два
    const uint32_t h = (2UL * factor + rate - 1) / rate;
    return h ? (uint16_t)h : 1;
}

AlarmThresholds alarm_thresholds(const bool vacation, const uint8_t counter_type,
                                 const uint16_t vol, const uint16_t rate,
                                 const uint16_t hours, const uint16_t factor)
{
    AlarmThresholds t = {0, 0, 0};

    if (!counts_impulses(counter_type))
    {
        return t;
    }

    if (vacation)
    {
        t.vol_pulses = 1; // любой импульс - тревога
        return t;
    }

    if (!factor_configured(factor))
    {
        return t;
    }

    t.vol_pulses = volume_to_pulses(vol, factor);
    t.quantum_ticks = rate_to_quantum_ticks(rate, factor);
    t.leak_quanta = hours_to_quanta(hours, rate, factor);

    // Без кванта протечки не существует: считать нечего
    if (t.quantum_ticks == 0)
    {
        t.leak_quanta = 0;
    }

    return t;
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

uint8_t alarm_flags_after_reset(const uint8_t attiny_flags, const uint8_t reset_mask)
{
    const uint8_t ch0 = reset_mask & ATTINY_ALARM_MASK;
    const uint8_t ch1 = (reset_mask >> ALARM_RESET_SHIFT1) & ATTINY_ALARM_MASK;

    return attiny_flags & (uint8_t)~((ch0 << ATTINY_ALARM_SHIFT0) |
                                     (ch1 << ATTINY_ALARM_SHIFT1));
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
