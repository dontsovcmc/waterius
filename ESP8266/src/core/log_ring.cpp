#include "log_ring.h"

#include <string.h>

// Остаток от деления у ЕСП программный, поэтому он один на вызов, а не на байт
void log_ring_write(LogRing &ring, const uint8_t *data, size_t len)
{
    if (!ring.buf || !ring.size)
        return;

    if (len > ring.size)
    {
        data += len - ring.size;
        ring.written += len - ring.size;
        len = ring.size;
    }

    const size_t at = ring.written % ring.size;
    const size_t first = len < ring.size - at ? len : ring.size - at;

    memcpy(ring.buf + at, data, first);
    memcpy(ring.buf, data + first, len - first);
    ring.written += len;
}

uint32_t log_ring_oldest(const LogRing &ring)
{
    return ring.written > ring.size ? ring.written - ring.size : 0;
}

size_t log_ring_read(const LogRing &ring, uint32_t &pos, uint32_t end, uint8_t *out, size_t max)
{
    if (!ring.buf || !ring.size)
        return 0;

    const uint32_t oldest = log_ring_oldest(ring);
    if (pos < oldest)
        pos = oldest;

    if (end > ring.written)
        end = ring.written;

    if (pos >= end)
        return 0;

    const size_t count = end - pos < max ? end - pos : max;
    const size_t at = pos % ring.size;
    const size_t first = count < ring.size - at ? count : ring.size - at;

    memcpy(out, ring.buf + at, first);
    memcpy(out + first, ring.buf, count - first);
    pos += count;
    return count;
}
