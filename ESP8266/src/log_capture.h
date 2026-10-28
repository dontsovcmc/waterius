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

/*
Начать журнал. Только в портале: в обычном сеансе эта память нужна HTTPS.
*/
bool log_capture_begin(size_t size);

const LogRing &log_capture();

#endif
