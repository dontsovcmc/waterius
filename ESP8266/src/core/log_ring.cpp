#include "log_ring.h"

void log_ring_write(LogRing &ring, const uint8_t *data, size_t len)
{
    if (!ring.buf || !ring.size)
        return;

    for (size_t i = 0; i < len; ++i)
    {
        ring.buf[ring.written % ring.size] = (char)data[i];
        ring.written++;
    }
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

    size_t count = 0;
    while (pos < end && count < max)
    {
        out[count++] = (uint8_t)ring.buf[pos % ring.size];
        pos++;
    }
    return count;
}
