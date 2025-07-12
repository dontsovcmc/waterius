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
        DDRB &= ~_BV(pin);   // Input
        PORTB &= ~_BV(_pin); // Disable pull-up
    }

    // Проверка нажатия кнопки
    bool pressed(CounterEvent event)
    {
        // Пока не обработано прошлое нажатие - кнопку не проверяем
        if (press != ButtonPressType::NONE)
        {
            return true;
        }

#if WATERIUS_MODEL == MODEL_CLASSIC 
        if (bit_is_set(PINB, _pin) == LOW)
#endif
#if WATERIUS_MODEL == MODEL_MINI 
        if (bit_is_set(PINB, _pin) == HIGH)
#endif
        {
            // Кнопка нажата
            if (on_time == 0)
            {
                // Начало нажатия
                on_time = 1;
                off_time = 0;
                LOG(F("PRESS_START"));
            }
            else
            {
                // Продолжение
                if (on_time < 200)
                {
                    // Увеличиваем счетчик времени в замкнутом состоянии
                    on_time += event == CounterEvent::TIME ? 10 : 1;
                }
                off_time = 0;
            }
        }
        else
        {
            // Отпущена
            if (on_time > 0)
            {
                LOG(F("PRESS_OFF"));
                // Идет обработка нажатия
                if (off_time < 200)
                {
                    // Увеличиваем счетчик времени в разомкнутом состоянии
                    off_time += event == CounterEvent::TIME ? 10 : 1;
                }
            }
        }

        // Определяем тип нажатия
        if ((on_time > 0) && (off_time > 20))
        {
            if (on_time > LONG_PRESS_MSEC / 25)
            {
                LOG(F("PRESS: LONG"));
                press = ButtonPressType::LONG;
            }
            else if (on_time > 10)
            {
                LOG(F("PRESS: SHORT"));
                press = ButtonPressType::SHORT;
            }
            on_time = off_time = 0;
        }

        return press != ButtonPressType::NONE;
    }

};

#endif