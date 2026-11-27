#include "connect_log.h"

#include <stdio.h>
#include <string.h>

namespace
{
    const char *const STAGE_NAMES[] = {"", "wifi", "wtr", "http", "mqtt", "ntp"};
    const size_t STAGE_COUNT = sizeof(STAGE_NAMES) / sizeof(STAGE_NAMES[0]);
}

void connect_log_push(Settings &sett, ConnectStage stage, int16_t code)
{
    memmove(sett.connect_log, sett.connect_log + 1, sizeof(ConnectError) * (CONNECT_LOG_SIZE - 1));
    sett.connect_log[CONNECT_LOG_SIZE - 1].stage = stage;
    sett.connect_log[CONNECT_LOG_SIZE - 1].code = code;
    sett.wifi_connect_errors++;
}

void connect_log_text(const Settings &sett, char *out, size_t size)
{
    size_t len = 0;
    out[0] = '\0';

    for (size_t i = 0; i < CONNECT_LOG_SIZE; ++i)
    {
        const ConnectError &error = sett.connect_log[i];
        if (error.stage == CONNECT_NONE || error.stage >= STAGE_COUNT)
        {
            continue;
        }

        const int code = error.code;
        len += snprintf(out + len, size - len, "%s%s:%d", len ? "," : "", STAGE_NAMES[error.stage], code);
    }
}
