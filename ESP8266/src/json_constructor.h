/**
 * @file json_constructor.h
 * @brief Формирование JSON строки с показаниями
 * @version 0.1
 * @date 2023-03-03
 * @author neitri
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef JSON_CONSTRUCTOR_h_
#define JSON_CONSTRUCTOR_h_

#include <ESP8266WiFi.h>
#include "WString.h"

class JsonConstructor
{
private:
    char *_buffer;
    bool _init;
    size_t _size, _pos;
    uint16_t write(const char* value);
    uint16_t write(const __FlashStringHelper* value);
    uint16_t write(const char *value, size_t size);
    uint16_t write(int32_t value);
    uint16_t write(uint32_t value);
    uint16_t write(double value, uint8_t size=2);
    uint16_t write(float value, uint8_t size=2);
    uint16_t write(bool value);
    uint16_t writeIP(uint32_t value);
    void insert(bool str=true);
public:
    JsonConstructor() = default;
    JsonConstructor(size_t size);
    ~JsonConstructor();
    void begin();
    void beginObject(const char* name);
    void beginObject(const __FlashStringHelper* name);
    void beginArray(const char* name);
    void beginArray(const __FlashStringHelper* name);
    void end();
    void endObject();
    void endArray();
    void setSize(size_t size);
    void push(const char* name, const char* value);
    void push(const char* name, const char* value, size_t size);
    void push(const char* name, const __FlashStringHelper* value);
    void push(const char* name, uint32_t value);
    void push(const char* name, uint16_t value);
    void push(const char* name, uint8_t value);
    void push(const char* name, int32_t value);
    void push(const char* name, int16_t value);
    void push(const char* name, int8_t value);
    void push(const char* name, double value, uint8_t size = 2);
    void push(const char* name, float value, uint8_t size = 2);
    void push(const char* name, bool value);
    void pushIP(const char* name, uint32_t value);

    
    void push(const __FlashStringHelper* name, const char* value);
    void push(const __FlashStringHelper* name, const char* value, size_t size);
    void push(const __FlashStringHelper* name, const __FlashStringHelper* value);
    void push(const __FlashStringHelper* name, uint32_t value);
    void push(const __FlashStringHelper* name, uint16_t value);
    void push(const __FlashStringHelper* name, uint8_t value);
    void push(const __FlashStringHelper* name, int32_t value);
    void push(const __FlashStringHelper* name, int16_t value);
    void push(const __FlashStringHelper* name, int8_t value);
    void push(const __FlashStringHelper* name, double value, uint8_t size);
    void push(const __FlashStringHelper* name, float value, uint8_t size);
    void push(const __FlashStringHelper* name, bool value);
    void pushIP(const __FlashStringHelper* name, uint32_t value);
    void push(const char* value);
    void push(const __FlashStringHelper* value);
    void push(uint32_t value);
    void push(uint16_t value);
    void push(uint8_t value);
    void push(int32_t value);
    void push(int16_t value);
    void push(int8_t value);
    void push(double value, uint8_t size);
    void push(float value, uint8_t size);
    const char* c_str(){
        return _buffer;
    }
    operator const char*(){
        return this->c_str();
    }
};

#endif