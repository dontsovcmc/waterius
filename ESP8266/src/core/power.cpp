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
    if (avg_mv <= W2_BATTERY_EMPTY_MV)
        return 0;
    if (avg_mv >= W2_BATTERY_FULL_MV)
        return 100;
    return (uint8_t)((uint32_t)(avg_mv - W2_BATTERY_EMPTY_MV) * 100 / (W2_BATTERY_FULL_MV - W2_BATTERY_EMPTY_MV));
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
