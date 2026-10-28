#include "power.h"

bool usb_powered(const uint16_t avg_mv)
{
    return avg_mv > USB_VOLTAGE_MV;
}

bool low_voltage(const uint16_t avg_mv, const bool was_low)
{
    const uint16_t threshold = was_low ? BATTERY_LOW_MV + BATTERY_LOW_HYSTERESIS_MV : BATTERY_LOW_MV;
    return avg_mv < threshold;
}

uint8_t battery_level_w2(const uint16_t avg_mv)
{
    // Точки шкалы: напряжение трёх элементов, мВ -> остаток, %
    static const struct
    {
        uint16_t mv;
        uint8_t level;
    } SCALE[] = {
        {2500, 0}, {2700, 3}, {3000, 8}, {3300, 20},
        {3600, 40}, {3900, 65}, {4200, 90}, {4500, 100},
    };
    const int last = sizeof(SCALE) / sizeof(SCALE[0]) - 1;

    if (avg_mv <= SCALE[0].mv)
        return 0;
    if (avg_mv >= SCALE[last].mv)
        return 100;

    int i = 1;
    while (avg_mv > SCALE[i].mv)
        i++;
    const uint32_t span_mv = SCALE[i].mv - SCALE[i - 1].mv;
    const uint32_t span_level = SCALE[i].level - SCALE[i - 1].level;
    return (uint8_t)(SCALE[i - 1].level + (avg_mv - SCALE[i - 1].mv) * span_level / span_mv);
}

uint8_t battery_level(const uint16_t avg_mv, const bool low)
{
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    (void)low;
    return battery_level_w2(avg_mv);
#else
    (void)avg_mv;
    return low ? 0 : 100;
#endif
}
