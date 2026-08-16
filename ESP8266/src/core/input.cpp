#include "input.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Буфер для замены запятой на точку. Поля ввода в интерфейсе заметно короче.
#define DECIMAL_BUF_LEN 64

bool is_all_asterisks(const char *value)
{
    if (value == nullptr || value[0] == 0)
        return false;

    for (size_t i = 0; value[i]; ++i)
    {
        char c = value[i];
        if (c != '*' && c != ' ' && c != '\t')
            return false;  // только *, пробел, таб
    }
    return true;
}

void copy_trimmed(char *dest, const char *src, size_t size)
{
    if (size == 0) return;

    // Пропускаем начальные пробелы
    while (*src && isspace((unsigned char)*src))
        src++;

    // Находим конец строки
    size_t len = strlen(src);

    // Обрезаем конечные пробелы
    while (len > 0 && isspace((unsigned char)src[len - 1]))
        len--;

    if (len >= size)
        len = size - 1;

    memcpy(dest, src, len);
    dest[len] = 0;
}

ParamError parse_text(char *dest, size_t size, const char *value, bool required)
{
    size_t len = strlen(value);

    if (len >= size)
        return PARAM_ERR_LENGTH;

    if (required && len == 0)
        return PARAM_ERR_EMPTY;

    if (is_all_asterisks(value))
        return PARAM_MASKED;

    copy_trimmed(dest, value, size);
    return PARAM_OK;
}

ParamError parse_decimal(const char *value, float &out)
{
    /* Позволяем вводить 0.0 у счётчиков, поэтому неверных значений тут нет */
    char buf[DECIMAL_BUF_LEN];
    size_t i = 0;
    for (; value[i] && i < sizeof(buf) - 1; ++i)
    {
        buf[i] = (value[i] == ',') ? '.' : value[i];
    }
    buf[i] = 0;

    out = (float)atof(buf);
    return PARAM_OK;
}

ParamError parse_uint16(const char *value, uint16_t &out)
{
    long v = atol(value);
    if (v == 0)
        return PARAM_ERR_VALUE;

    out = v;
    return PARAM_OK;
}

ParamError parse_uint8(const char *value, uint8_t &out, const bool zero_ok)
{
    long v = atol(value);
    if (!zero_ok && v == 0)
        return PARAM_ERR_VALUE;

    out = v;
    return PARAM_OK;
}

ParamError parse_bool(const char *value, uint8_t &out)
{
    long v = atol(value);
    if (v > 1)
        return PARAM_ERR_VALUE;

    out = v;
    return PARAM_OK;
}
