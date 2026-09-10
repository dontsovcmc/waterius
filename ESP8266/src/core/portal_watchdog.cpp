#include "portal_watchdog.h"

bool portal_watchdog_fired(const uint32_t now_ms, const uint32_t last_feed_ms)
{
    return (uint32_t)(now_ms - last_feed_ms) >= PORTAL_WATCHDOG_MS;
}

uint32_t portal_idle_seconds_left(const uint32_t now_ms, const uint32_t last_feed_ms)
{
    const uint32_t passed = (uint32_t)(now_ms - last_feed_ms);

    if (passed >= PORTAL_WATCHDOG_MS)
        return 0;

    return (PORTAL_WATCHDOG_MS - passed) / 1000UL;
}
