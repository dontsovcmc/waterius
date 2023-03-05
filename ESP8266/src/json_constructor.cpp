#include "json_constructor.h"

void JsonConstructor::begin()
{
    _pos = 0;
    _init = false;
    *_buffer = '\0';
    write(F("{"));
}

void JsonConstructor::beginObject(const char *name)
{
    insert();
    write(name);
    write(F("\":{"));
    _init = false;
}

void JsonConstructor::beginObject(const __FlashStringHelper *name)
{
    insert();
    write(name);
    write(F("\":{"));
    _init = false;
}

void JsonConstructor::beginArray(const char *name)
{
    insert();
    write(name);
    write(F("\":["));
    _init = false;
}

void JsonConstructor::beginArray(const __FlashStringHelper *name)
{
    insert();
    write(name);
    write(F("\":["));
    _init = false;
}

void JsonConstructor::end()
{
    write(F("}"));
}

void JsonConstructor::endObject()
{
    write(F("}"));
}

void JsonConstructor::endArray()
{
    write(F("]"));
}

uint16_t JsonConstructor::write(const __FlashStringHelper *value)
{
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

uint16_t JsonConstructor::write(const char *value)
{
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

uint16_t JsonConstructor::write(const char *value, size_t size)
{
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

uint16_t JsonConstructor::write(int32_t value)
{
    char buf[11];
    itoa(value, buf, DEC);
    return write(buf);
}

uint16_t JsonConstructor::write(uint32_t value)
{
    char buf[11];
    utoa(value, buf, DEC);
    return write(buf);
}

uint16_t JsonConstructor::write(double value, uint8_t size)
{
    char buf[20];
    dtostrf(value, -19, size, buf);
    return write(buf);
}

uint16_t JsonConstructor::write(float value, uint8_t size)
{
    return write((double)value, size);
}

uint16_t JsonConstructor::write(bool value)
{
    if(value)
        return write(F("true"));
    return write(F("false"));
}

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
    if(str){
        write(F("\""));
    }
}

void JsonConstructor::push(const char *name, const char *value)
{
    insert();
    write(name);
    write(F("\":\""));
    write(value);
    write(F("\""));
}

void JsonConstructor::push(const __FlashStringHelper *name, const char *value)
{
    insert();
    write(name);
    write(F("\":\""));
    write(value);
    write(F("\""));
}

void JsonConstructor::push(const char *name, const __FlashStringHelper *value)
{
    insert();
    write(name);
    write(F("\":\""));
    write(value);
    write(F("\""));
}

void JsonConstructor::push(const __FlashStringHelper *name, const __FlashStringHelper *value)
{
    insert();
    write(name);
    write(F("\":\""));
    write(value);
    write(F("\""));
}

void JsonConstructor::push(const char *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

void JsonConstructor::push(const __FlashStringHelper *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

void JsonConstructor::push(const char *name, uint16_t value)
{
    push(name, (uint32_t)value);
}

void JsonConstructor::push(const __FlashStringHelper *name, uint16_t value)
{
    push(name, (uint32_t)value);
}

void JsonConstructor::push(const char *name, uint8_t value)
{
    push(name, (uint32_t)value);
}

void JsonConstructor::push(const __FlashStringHelper *name, uint8_t value)
{
    push(name, (uint32_t)value);
}

void JsonConstructor::push(const char *name, int32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

void JsonConstructor::push(const __FlashStringHelper *name, int32_t value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

void JsonConstructor::push(const char *name, int16_t value)
{
    push(name, (int32_t)value);
}

void JsonConstructor::push(const __FlashStringHelper *name, int16_t value)
{
    push(name, (int32_t)value);
}

void JsonConstructor::push(const char *name, int8_t value)
{
    push(name, (int32_t)value);
}

void JsonConstructor::push(const __FlashStringHelper *name, int8_t value)
{
    push(name, (int32_t)value);
}

void JsonConstructor::push(const char *name, const char *value, size_t size)
{
    insert();
    write(name);
    write(F("\":\""));
    write(value, size);
    write(F("\""));
}

void JsonConstructor::push(const __FlashStringHelper *name, const char *value, size_t size)
{
    insert();
    write(name);
    write(F("\":\""));
    write(value, size);
    write(F("\""));
}

void JsonConstructor::push(const char *name, double value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

void JsonConstructor::push(const __FlashStringHelper *name, double value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

void JsonConstructor::push(const char *name, float value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

void JsonConstructor::push(const __FlashStringHelper *name, float value, uint8_t size)
{
    insert();
    write(name);
    write(F("\":"));
    write(value, size);
}

void JsonConstructor::push(const char *name, bool value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

void JsonConstructor::push(const __FlashStringHelper *name, bool value)
{
    insert();
    write(name);
    write(F("\":"));
    write(value);
}

void JsonConstructor::pushIP(const char *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":\""));
    writeIP(value);
    write(F("\""));
}

void JsonConstructor::push(const char *value)
{
    insert();
    write(value);
    write(F("\""));
}

void JsonConstructor::push(const __FlashStringHelper *value)
{
    insert();
    write(value);
    write(F("\""));
}

void JsonConstructor::push(uint32_t value)
{
    insert(false);
    write(value);
}
void JsonConstructor::push(uint16_t value)
{
    push((uint32_t)value);
}

void JsonConstructor::push(uint8_t value)
{
    push((uint32_t)value);
}

void JsonConstructor::push(int32_t value)
{
    insert(false);
    write(value);
}

void JsonConstructor::push(int16_t value)
{
    push((int32_t)value);
}

void JsonConstructor::push(int8_t value)
{
    push((int32_t)value);
}

void JsonConstructor::push(double value, uint8_t size)
{
    insert(false);
    write(value, size);
}

void JsonConstructor::push(float value, uint8_t size)
{
    insert(false);
    write(value, size);
}

void JsonConstructor::pushIP(const __FlashStringHelper *name, uint32_t value)
{
    insert();
    write(name);
    write(F("\":\""));
    writeIP(value);
    write(F("\""));
}

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

JsonConstructor::JsonConstructor(size_t size)
{
    _pos = 0;
    _size = size;
    _init = false;
    _buffer = (char *)malloc(size);
    if (_buffer)
        *_buffer = '\0';
}

JsonConstructor::~JsonConstructor()
{
    if (_buffer)
    {
        delete _buffer;
    }
}