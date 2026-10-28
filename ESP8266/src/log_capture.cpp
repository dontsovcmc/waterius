#include "log_capture.h"

#include <Arduino.h>
#include <stdlib.h>

LogOutput log_output;

static LogRing ring;

size_t LogOutput::write(uint8_t c)
{
    log_ring_write(ring, &c, 1);
    return Serial.write(c);
}

size_t LogOutput::write(const uint8_t *buffer, size_t size)
{
    log_ring_write(ring, buffer, size);
    return Serial.write(buffer, size);
}

bool log_capture_begin(size_t size)
{
    if (ring.buf)
        return true;

    ring.buf = (char *)malloc(size);
    if (!ring.buf)
        return false;

    ring.size = size;
    return true;
}

const LogRing &log_capture()
{
    return ring;
}
