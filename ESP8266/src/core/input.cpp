#include "input.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
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

/*
Целое из строки формы или MQTT, с проверкой диапазона.

atol, который стоял здесь раньше, годился только на вид: он не отличает
"abc" от нуля, срезает переполнение молча — 70000 в uint16_t превращалось
в 4464, то есть порт MQTT сохранялся другим, а портал отвечал "сохранено",
— и пропускает отрицательное, которое при записи в беззнаковое поле
становится большим положительным: "-1" во флажке давало 255, то есть
"включено".

Поэтому strtol и три проверки: цифры вообще были, после числа нет мусора,
результат влезает в допустимые границы. Хвостовые пробелы разрешены -
значение приходит из формы как есть.
*/
static bool parse_long(const char *value, const long min, const long max, long &out)
{
    if (value == nullptr)
        return false;

    char *end = nullptr;
    errno = 0;
    const long v = strtol(value, &end, 10);

    if (end == value)
        return false;  // ни одной цифры

    while (*end && isspace((unsigned char)*end))
        end++;

    if (*end)
        return false;  // после числа что-то ещё

    if (errno == ERANGE || v < min || v > max)
        return false;

    out = v;
    return true;
}

ParamError parse_uint16(const char *value, uint16_t &out, const bool zero_ok)
{
    long v = 0;
    // По умолчанию ноль — ошибка: это и незаполненное поле, и мусор. Явное
    // разрешение нужно там, где ноль значит "выключено" (пороги тревог, #202)
    if (!parse_long(value, zero_ok ? 0 : 1, 65535, v))
        return PARAM_ERR_VALUE;

    out = (uint16_t)v;
    return PARAM_OK;
}

ParamError parse_uint8(const char *value, uint8_t &out, const bool zero_ok)
{
    long v = 0;
    if (!parse_long(value, zero_ok ? 0 : 1, 255, v))
        return PARAM_ERR_VALUE;

    out = (uint8_t)v;
    return PARAM_OK;
}

ParamError parse_bool(const char *value, uint8_t &out)
{
    long v = 0;
    if (!parse_long(value, 0, 1, v))
        return PARAM_ERR_VALUE;

    out = (uint8_t)v;
    return PARAM_OK;
}

bool is_valid_counter_type(const uint8_t counter_type)
{
    switch (counter_type)
    {
        case CounterType::NAMUR:
        case CounterType::DISCRETE:
        case CounterType::ELECTRONIC:
        case CounterType::HALL:
        case CounterType::ELECTRONIC_HIGH:
        case CounterType::NONE:
            return true;
    }
    return false;
}

bool is_water_counter(const uint8_t counter_name)
{
    return counter_name == CounterName::WATER_COLD
        || counter_name == CounterName::WATER_HOT
        || counter_name == CounterName::PORTABLE_WATER;
}

bool has_decimal_separator(const char *value)
{
    return strchr(value, '.') != nullptr || strchr(value, ',') != nullptr;
}

ParamError check_reading(const char *value, const uint8_t counter_name)
{
    if (is_water_counter(counter_name) && !has_decimal_separator(value))
        return PARAM_ERR_NO_COMMA;

    return PARAM_OK;
}
