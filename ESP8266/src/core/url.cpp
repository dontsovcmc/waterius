#include "url.h"

#include <ctype.h>
#include <string.h>

#define SCHEME_SEPARATOR "://"

namespace
{
    // Сравнение схемы без учёта регистра: MQTT:// и mqtt:// одинаковы
    bool scheme_equals(const char *value, const size_t len, const char *scheme)
    {
        if (strlen(scheme) != len)
            return false;

        for (size_t i = 0; i < len; ++i)
        {
            if (tolower((unsigned char)value[i]) != scheme[i])
                return false;
        }
        return true;
    }

    bool is_plain_tcp_scheme(const char *value, const size_t len)
    {
        return scheme_equals(value, len, "mqtt")
            || scheme_equals(value, len, "tcp")
            || scheme_equals(value, len, "http");
    }

    bool is_encrypted_scheme(const char *value, const size_t len)
    {
        return scheme_equals(value, len, "mqtts")
            || scheme_equals(value, len, "ssl")
            || scheme_equals(value, len, "tls")
            || scheme_equals(value, len, "wss")
            || scheme_equals(value, len, "https");
    }
}

ParamError parse_broker_host(char *dest, const size_t size, const char *value)
{
    // Пробелы по краям обрежет parse_text, но ведущие надо снять сразу:
    // иначе они попадут в схему и "  mqtt://" не совпадёт ни с одной
    while (*value && isspace((unsigned char)*value))
        value++;

    const char *host = value;

    // Схема, если она есть
    const char *separator = strstr(value, SCHEME_SEPARATOR);
    if (separator != nullptr)
    {
        const size_t scheme_len = separator - value;

        if (is_encrypted_scheme(value, scheme_len))
            return PARAM_ERR_TLS;

        if (!is_plain_tcp_scheme(value, scheme_len))
            return PARAM_ERR_VALUE;

        host = separator + strlen(SCHEME_SEPARATOR);
    }

    // Путь отбрасываем: для MQTT поверх TCP он ничего не значит
    char buffer[HOST_LEN];
    const char *path = strchr(host, '/');
    if (path != nullptr)
    {
        size_t host_len = path - host;
        if (host_len >= sizeof(buffer))
            return PARAM_ERR_LENGTH;

        memcpy(buffer, host, host_len);
        buffer[host_len] = 0;
        host = buffer;
    }

    // Порт менять молча нельзя: подключились бы не туда, куда хотел пользователь
    if (strchr(host, ':') != nullptr)
        return PARAM_ERR_PORT_IN_HOST;

    return parse_text(dest, size, host, true);
}
