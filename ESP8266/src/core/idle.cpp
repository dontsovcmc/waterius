#include "idle.h"

bool consumption_detected(const uint32_t impulses0, const uint32_t impulses0_previous,
                          const uint32_t impulses1, const uint32_t impulses1_previous)
{
    return impulses0 != impulses0_previous || impulses1 != impulses1_previous;
}

uint16_t add_minutes(const uint16_t accumulated, const uint16_t minutes)
{
    const uint32_t sum = (uint32_t)accumulated + minutes;

    return sum > 0xFFFF ? (uint16_t)0xFFFF : (uint16_t)sum;
}

bool need_transmit(const bool send_on_consumption, const bool consumed,
                   const uint16_t minutes_since_send)
{
    if (!send_on_consumption || consumed)
        return true;

    return (uint32_t)minutes_since_send >= MAX_SILENCE_MIN;
}
