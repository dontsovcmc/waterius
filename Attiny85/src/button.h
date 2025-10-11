#ifndef _BUTTON_h
#define _BUTTON_h

#include <Arduino.h>
#include <avr/wdt.h>
#include "counter.h"

#if defined(LOG_ON)
extern TinyDebugSerial mySerial;
#endif

enum class ButtonPressType
{
    NONE,
    SHORT,
    LONG,
};

struct ButtonB
{
    uint8_t         _pin; // дискретный вход

    uint8_t         on_time;
    uint8_t         off_time;

    ButtonPressType press;

    explicit ButtonB(uint8_t pin)
        : _pin(pin), on_time(0), off_time(0), press(ButtonPressType::NONE)
    {
        init();
    }

    void init()
    {
        press = ButtonPressType::NONE;
        DDRB &= ~_BV(_pin);    // Input
        PORTB &= ~_BV(_pin);   // Убедиться что pull-up выключен
    }

    // Проверка нажатия кнопки
    bool pressed(CounterEvent event)
    {
        if (bit_is_set(PINB, _pin) == LOW)
        {
            press = ButtonPressType::SHORT;
            return true;
        }
        
        return false;
    }

};

#endif