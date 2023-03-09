#include "json_constructor.h"

/**
 * @brief Начинает формирование json заново.
 * 
 * Удаляет содержимое и вставляет { вначале строки
 */
void JsonConstructor::begin()
{
    _pos = 0;
    _init = false;
    *_buffer = '\0';
    write(F("{"));
}

/**
 * @brief Начинает описание нового объекта с заданным именем
 * 
 * @param name имя объекта
 */
void JsonConstructor::beginObject(const char *name)
{
    insert();
    write(name);
    write(F("\":{"));
    _init = false;
}

/**
 * @brief Начинает описание нового объекта с заданным именем
 * 
 * @param name имя объекта
 */
void JsonConstructor::beginObject(const __FlashStringHelper *name)
{
    insert();
    write(name);
    write(F("\":{"));
    _init = false;
}

/**
 * @brief Начинает описание нового массива с заданным именем
 * 
 * @param name имя массива
 */
void JsonConstructor::beginArray(const char *name)
{
    insert();
    write(name);
    write(F("\":["));
    _init = false;
}

/**
 * @brief Начинает описание нового массива с заданным именем
 * 
 * @param name имя массива
 */
void JsonConstructor::beginArray(const __FlashStringHelper *name)
{
    insert();
    write(name);
    write(F("\":["));
    _init = false;
}

/**
 * @brief Завершает документ
 */
void JsonConstructor::end()
{
    write(F("}"));
}

/**
 * @brief Завершает описание объекта
 */
void JsonConstructor::endObject()
{
    write(F("}"));
}

/**
 * @brief Завершает описание массива
 */
void JsonConstructor::endArray()
{
    write(F("]"));
}

/**
 * @brief записывает в документ текст из памяти программ
 * 
 * @param value указатель на строку в памяти программы
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(const __FlashStringHelper *value)
{
    if (value == nullptr)
        return 0;
    PGM_P src = reinterpret_cast<PGM_P>(value);
    char *dst = _buffer + _pos;
    if ((_buffer) && (strlen_P(src) + _pos < _size))
    {
        strcpy_P(dst, src);
        _pos += strlen_P(src);
        return strlen_P(src);
    }
    return 0;
}

/**
 * @brief записывает в документ текст из памяти
 * 
 * @param value указатель на строку в памяти
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(const char *value)
{
    if (value == nullptr)
        return 0;
    const char *src = value;
    char *dst = _buffer + _pos;
    if ((_buffer) && (strlen(value) + _pos < _size))
    {
        strcpy(dst, src);
        _pos += strlen(value);
        return strlen(src);
    }
    return 0;
}

/**
 * @brief записывает в документ текст из памяти заданной длинны
 * 
 * @param value указатель на строку в памяти
 * @param size количество символов для записи
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(const char *value, size_t size)
{
    if (value == nullptr)
        return 0;
    const char *src = value;
    char *dst = _buffer + _pos;
    if ((_buffer) && (size + _pos < _size))
    {
        strncpy(dst, src, size);
        _pos += size;
        return strlen(src);
    }
    return 0;
}

/**
 * @brief записывает число в документ
 * 
 * @param value значение 
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(int32_t value)
{
    char buf[11];
    itoa(value, buf, DEC);
    return write(buf);
}

/**
 * @brief записывает число в документ
 * 
 * @param value значение 
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(uint32_t value)
{
    char buf[11];
    utoa(value, buf, DEC);
    return write(buf);
}

/**
 * @brief записывает число в документ с заданной точностью
 * 
 * @param value значение 
 * @param size точность
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(double value, uint8_t size)
{
    char buf[20];
    dtostrf(value, -19, size, buf);
    return write(buf);
}

/**
 * @brief записывает число в документ с заданной точностью
 * 
 * @param value значение 
 * @param size точность
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(float value, uint8_t size)
{
    return write((double)value, size);
}

/**
 * @brief записывает булевое значение в документ
 * 
 * @param value значение 
  * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::write(bool value)
{
    if (value)
        return write(F("true"));
    return write(F("false"));
}

/**
 * @brief записывает в документ строку с экранирующими символами
 * 
 * @param value значение
 * @param size количество символов
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::quoted(const char *value, size_t size)
{
    if (value == nullptr)
        return 0;
    const char *src = value;
    char *dst = _buffer + _pos;
    if ((_buffer) && (size + _pos < _size))
    {
        size_t q = 0;
        for (size_t i = size; i > 0; i--)
        {
            if (*src == '\"')
            {
                *dst++ = '\\';
                q++;
            }
            *dst++ = *src++;
        }
        _pos += size + q;
        return size + q;
    }
    return 0;
}

/**
 * @brief записывает в документ строку с экранирующими символами
 * 
 * @param value значение
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::quoted(const char *value)
{
    return quoted(value, strlen(value));
}

/**
 * @brief записывает в документ строку из памяти программ с экранирующими символами заданной длинны
 * 
 * @param value значение
 * @param size количество символов
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::quoted(const __FlashStringHelper *value, size_t size)
{
    if (value == nullptr)
        return 0;
    PGM_P src = reinterpret_cast<PGM_P>(value);
    char *dst = _buffer + _pos;
    if ((_buffer) && (size + _pos < _size))
    {
        size_t q = 0;
        for (size_t i = size; i > 0; i--)
        {
            char c;
            strncpy_P(&c, src++, 1);
            if (c == '\"')
            {
                *dst++ = '\\';
                q++;
            }
            *dst++ = c;
        }
        _pos += size + q;
        return size + q;
    }
    return 0;
}

/**
 * @brief записывает в документ строку из памяти программ с экранирующими символами
 * 
 * @param value значение
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::quoted(const __FlashStringHelper *value)
{
    return quoted(value, strlen_P(reinterpret_cast<PGM_P>(value)));
}

/**
 * @brief записывает в документ текстовое предстовление ip адреса
 * 
 * @param value значение
 * @return uint16_t количество записанных байт
 */
uint16_t JsonConstructor::writeIP(uint32_t value)
{
    uint16_t ret = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        ret += write((uint32_t)((uint8_t *)&value)[i]);
        if (i != 3)
            ret += write(F("."));
    }
    return ret;
}

/**
 * @brief вставляет раделитель между параметрами и " для текстовых данных
 * 
 * @param str флаг текстовых данных
 */
void JsonConstructor::insert(bool str)
{
    if (_init)
    {
        write(F(","));
    }
    else
    {
        _init = true;
    }
    if (str)
    {
        write(F("\""));
    }
}

/**
 * @brief Добавляет в документ строковый параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, const char *value)
{
    insert();
    write(name);
    write(F("\":\""));
    quoted(value, strlen(value));
    write(F("\""));
}

/**
 * @brief Добавляет в документ строковый параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, const char *value)
{
    insert();
    write(name);
    write(F("\":\""));
    quoted(value, strlen(value));
    write(F("\""));
}

/**
 * @brief Добавляет в документ строковый параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, const __FlashStringHelper *value)
{
    insert();
    write(name);
    write(F("\":\""));
    quoted(value, strlen_P(reinterpret_cast<PGM_P>(value)));
    write(F("\""));
}

/**
 * @brief Добавляет в документ строковый параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, const __FlashStringHelper *value)
{
    insert();
    write(name);
    write(F("\":\""));
    quoted(value, strlen_P(reinterpret_cast<PGM_P>(value)));
    write(F("\""));
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, uint16_t value)
{
    push(name, (uint32_t)value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, uint16_t value)
{
    push(name, (uint32_t)value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, uint8_t value)
{
    push(name, (uint32_t)value);
}

void JsonConstructor::push(const __FlashStringHelper *name, uint8_t value)
{
    push(name, (uint32_t)value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, int32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, int32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, int16_t value)
{
    push(name, (int32_t)value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, int16_t value)
{
    push(name, (int32_t)value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, int8_t value)
{
    push(name, (int32_t)value);
}

/**
 * @brief Добавляет в документ числовой параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, int8_t value)
{
    push(name, (int32_t)value);
}

/**
 * @brief Добавляет в документ строковый параметр с заданной длинной
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, const char *value, size_t size)
{
    insert();
    write(name);
    write(F("\":\""));
    quoted(value, size);
    write(F("\""));
}

/**
 * @brief Добавляет в документ строковый параметр с заданной длинной
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, const char *value, size_t size)
{
    insert();
    write(name);
    write(F("\":\""));
    quoted(value, size);
    write(F("\""));
}

/**
 * @brief Добавляет в документ числовой параметр с заданной точностью
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, double value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

/**
 * @brief Добавляет в документ числовой параметр с заданной точностью
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, double value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

/**
 * @brief Добавляет в документ числовой параметр с заданной точностью
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, float value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

/**
 * @brief Добавляет в документ числовой параметр с заданной точностью
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, float value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

/**
 * @brief Добавляет в документ билевый параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const char *name, bool value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

/**
 * @brief Добавляет в документ билевый параметр
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::push(const __FlashStringHelper *name, bool value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

/**
 * @brief Добавляет в документ параметр с ip адресом
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::pushIP(const char *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":\""));
    writeIP(value);
    write(F("\""));
}

/**
 * @brief Добавляет в документ строку
 * 
 * @param value значение
 */
void JsonConstructor::push(const char *value)
{
    insert();
    write(value);
    write(F("\""));
}

/**
 * @brief Добавляет в документ строку
 * 
 * @param value значение
 */
void JsonConstructor::push(const __FlashStringHelper *value)
{
    insert();
    write(value);
    write(F("\""));
}

/**
 * @brief Добавляет в документ число
 * 
 * @param value значение
 */
void JsonConstructor::push(uint32_t value)
{
    insert(false);
    write(value);
}

/**
 * @brief Добавляет в документ число
 * 
 * @param value значение
 */
void JsonConstructor::push(uint16_t value)
{
    push((uint32_t)value);
}

/**
 * @brief Добавляет в документ число
 * 
 * @param value значение
 */
void JsonConstructor::push(uint8_t value)
{
    push((uint32_t)value);
}

/**
 * @brief Добавляет в документ число
 * 
 * @param value значение
 */
void JsonConstructor::push(int32_t value)
{
    insert(false);
    write(value);
}

/**
 * @brief Добавляет в документ число
 * 
 * @param value значение
 */
void JsonConstructor::push(int16_t value)
{
    push((int32_t)value);
}

/**
 * @brief Добавляет в документ число
 * 
 * @param value значение
 */
void JsonConstructor::push(int8_t value)
{
    push((int32_t)value);
}

/**
 * @brief Добавляет в документ число с заданной точностью
 * 
 * @param value значение
 */
void JsonConstructor::push(double value, uint8_t size)
{
    insert(false);
    write(value, size);
}

/**
 * @brief Добавляет в документ число с заданной точностью
 * 
 * @param value значение
 */
void JsonConstructor::push(float value, uint8_t size)
{
    insert(false);
    write(value, size);
}

/**
 * @brief Добавляет в документ параметр с ip адресом
 * 
 * @param name имя параметра
 * @param value значение параметра
 */
void JsonConstructor::pushIP(const __FlashStringHelper *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":\""));
    writeIP(value);
    write(F("\""));
}

/**
 * @brief резервирует память для буффера
 * 
 * @param size размер буффера
 */
void JsonConstructor::setSize(size_t size)
{
    char *t = (char *)realloc(_buffer, size);
    if (t)
    {
        _buffer = t;
        *_buffer = '\0';
    }
    _pos = 0;
    _size = size;
}

/**
 * @brief Construct a new Json Constructor:: Json Constructor object
 * 
 * @param size размер буффера
 */
JsonConstructor::JsonConstructor(size_t size)
{
    _pos = 0;
    _size = size;
    _init = false;
    _buffer = (char *)malloc(size);
    if (_buffer)
        *_buffer = '\0';
}

/**
 * @brief Destroy the Json Constructor:: Json Constructor object
 * 
 */
JsonConstructor::~JsonConstructor()
{
    if (_buffer)
    {
        delete _buffer;
    }
}