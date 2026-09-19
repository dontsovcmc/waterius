#include "power.h"

#include <math.h>

bool usb_powered(const uint16_t avg_mv)
{
    return avg_mv > USB_VOLTAGE_MV;
}

bool low_voltage(const uint16_t avg_mv, const uint16_t diff_mv)
{
    if (usb_powered(avg_mv))
        return false;

    return (diff_mv >= ALERT_POWER_DIFF_MV) || (avg_mv < BATTERY_LOW_THRESHOLD_MV);
}

uint8_t battery_level(const uint16_t avg_mv, const uint16_t diff_mv)
{
    // https://mysku.club/blog/diy/81535.html - поведение батареек при разрядке
    if (usb_powered(avg_mv))
        return 100;

    if (low_voltage(avg_mv, diff_mv))
        return 0;

    if (diff_mv > LOW_BATTERY_DIFF_MV)
        return (uint8_t)round((ALERT_POWER_DIFF_MV - diff_mv) * 0.5);

    return (uint8_t)(25 + round((LOW_BATTERY_DIFF_MV - diff_mv) * 1.5));
}
