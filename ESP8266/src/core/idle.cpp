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

uint16_t update_idle_minutes(const bool consumed, const uint16_t accumulated,
                             const uint16_t minutes)
{
    if (consumed)
    {
        return 0;
    }
    return add_minutes(accumulated, minutes);
}

bool consumption_stopped(const uint16_t idle_min, const uint16_t stop_hours)
{
    if (stop_hours == 0)
    {
        return false; // тревога выключена
    }

    // Считаем в 32 битах: часы, умноженные на 60, из uint16_t выходят уже на
    // 1093-м часе. Порог выше ALARM_STOP_MAX_HOURS отсекается при сохранении,
    // но арифметика не должна зависеть от того, что кто-то это проверил
    const uint32_t stop_min = (uint32_t)stop_hours * 60;

    return (uint32_t)idle_min >= stop_min;
}
