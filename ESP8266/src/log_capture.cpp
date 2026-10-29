#include "log_capture.h"
#include "Logging.h"

#include <Arduino.h>
#include <stdlib.h>

LogOutput log_output;

static LogRing ring;
static bool line_open = false;

void log_head(LogTag tag)
{
    if (line_open)
    {
        // Ошибку видно и внутри строки: её ищут глазами и стенд
        log_output.print(tag == LogTag::error ? F("  ERROR : ") : F(" "));
        return;
    }

    LOG_FORMAT_TIME;
    LOG_FREE_HEAP;
    switch (tag)
    {
    case LogTag::error:
        log_output.print(F("  ERROR : "));
        break;
    case LogTag::debug:
        log_output.print(F("  DEBUG : "));
        break;
    default:
        log_output.print(F("  INFO  : "));
    }
}

void log_tail()
{
    if (!line_open)
        log_output.print(F("\r\n"));
}

LogLine::LogLine()
{
    line_open = true;
}

LogLine::~LogLine()
{
    line_open = false;
    log_output.print(F("\r\n"));
}

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
