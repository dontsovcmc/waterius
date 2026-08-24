#include "portal_watchdog.h"

bool portal_deadline_reached(const uint32_t now_ms, const uint32_t started_ms)
{
    return (uint32_t)(now_ms - started_ms) >= PORTAL_DEADLINE_MS;
}

bool portal_watchdog_fired(const uint32_t now_ms, const uint32_t started_ms, const uint32_t last_fed_ms)
{
    if (portal_deadline_reached(now_ms, started_ms))
        return true;

    return (uint32_t)(now_ms - last_fed_ms) >= PORTAL_WATCHDOG_MS;
}
