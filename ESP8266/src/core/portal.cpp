#include "portal.h"

bool portal_time_limit_reached(const uint32_t now_ms, const uint32_t started_ms)
{
    return (uint32_t)(now_ms - started_ms) >= PORTAL_MAX_MS;
}

bool portal_expired(const uint32_t now_ms, const uint32_t started_ms, const uint32_t last_activity_ms)
{
    if (portal_time_limit_reached(now_ms, started_ms))
        return true;

    return (uint32_t)(now_ms - last_activity_ms) >= PORTAL_IDLE_MS;
}
