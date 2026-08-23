#ifndef _WATERIUS_CORE_INPUT_h
#define _WATERIUS_CORE_INPUT_h

/*
Правила проверки того, что пользователь ввёл в веб-интерфейсе или прислал
по MQTT: что считать корректным значением, что — ошибкой, а что — просьбой
оставить поле как есть.

Часть чистого ядра src/core: без Arduino.h и логирования. Адаптеры
save_param() в portal/active_point_api.cpp распаковывают AsyncWebParameter,
зовут ядро и раскладывают код ошибки в JSON-ответ.

Разбор IP-адреса сюда не переехал: его делает IPAddress::fromString() из
Arduino, это чужая реализация, а не наше правило.
*/

#include "types.h"

/*
Коды ошибок. Числа те же, что уходят в JSON и разбираются веб-интерфейсом
в data/static, менять их нельзя.
*/
enum ParamError : uint8_t
{
    PARAM_OK = 0,
    PARAM_ERR_LENGTH = 14,   // превышена длина поля
    PARAM_ERR_VALUE = 15,    // неверное значение
    PARAM_ERR_EMPTY = 17,    // значение не может быть пустым
    PARAM_ERR_NO_COMMA = 19,     // похоже, забыта запятая
    PARAM_ERR_TLS = 20,          // шифрованное подключение не поддерживается
    PARAM_ERR_PORT_IN_HOST = 21, // порт указан в адресе, а не в своём поле
    PARAM_MASKED = 100           // пришли одни звёздочки: поле не редактировали
};

/*
Пришли одни звёздочки (плюс пробелы) — интерфейс так показывает пароль,
который пользователь не менял. Пустая строка звёздочками не считается,
иначе поле нельзя было бы очистить.
*/
bool is_all_asterisks(const char *value);

/*
Копирует src в dest, обрезая пробелы по краям. Без аллокаций на heap.
*/
void copy_trimmed(char *dest, const char *src, size_t size);

/*
Текстовое поле: проверка длины и пустоты, обрезка пробелов.
Длина проверяется до обрезки — строка из пробелов длиннее буфера
считается слишком длинной.
*/
ParamError parse_text(char *dest, size_t size, const char *value, bool required);

/*
Дробное число: запятая и точка равноправны как десятичный разделитель.
Разбор ошибок не возвращает — непонятный текст превращается в 0.0.
*/
ParamError parse_decimal(const char *value, float &out);

/*
Целое: ноль считается ошибкой.
*/
ParamError parse_uint16(const char *value, uint16_t &out);

ParamError parse_uint8(const char *value, uint8_t &out, const bool zero_ok);

/*
Флажок: допустимы только 0 и 1.
*/
ParamError parse_bool(const char *value, uint8_t &out);

bool is_water_counter(const uint8_t counter_name);

/*
Есть ли в строке десятичный разделитель — точка или запятая.
*/
bool has_decimal_separator(const char *value);

/*
Проверка показаний, введённых пользователем.

У водяного счётчика на шкале всегда есть литры, поэтому целое число в этом
поле означает, что пользователь переписал шкалу подряд: 123456 вместо
123.456. Проверяется именно строка, а не разобранное число: величина
показаний ничего не говорит об опечатке — 12345 бывает и настоящей
шкалой большого счётчика, и забытой запятой в 12.345.

Правило только для воды. У электричества целые кВт*ч — обычное дело,
показания газовых и тепловых счётчиков тоже не режем.
*/
ParamError check_reading(const char *value, const uint8_t counter_name);

#endif
