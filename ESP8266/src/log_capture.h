#ifndef _WATERIUS_LOG_CAPTURE_h
#define _WATERIUS_LOG_CAPTURE_h

#include <Print.h>
#include "core/log_ring.h"

/*
Вывод макросов LOG_*: UART и, в портале, журнал в памяти (core/log_ring.h).
*/
class LogOutput : public Print
{
public:
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
};

extern LogOutput log_output;

enum class LogTag : uint8_t
{
    info,
    error,
    debug
};

// Приставка и конец записи LOG_*
void log_head(LogTag tag);
void log_tail();

/*
Пока объект жив, LOG_* дописываются в начатую строку, а не начинают свою:
запрос портала в журнале занимает одну строку с одной приставкой.
*/
class LogLine
{
public:
    LogLine();
    ~LogLine();
};

/*
Начать журнал. Только в портале: в обычном сеансе эта память нужна HTTPS.
*/
bool log_capture_begin(size_t size);

const LogRing &log_capture();

#endif
