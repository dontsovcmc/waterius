#ifndef _BUTTON_h
#define _BUTTON_h

#include <Arduino.h>
#include <avr/wdt.h>
#include "counter.h"

enum class ButtonPressType
{
    NONE,
    SHORT,
    LONG,
};


#if WATERIUS_MODEL == WATERIUS_MODEL_1

struct ButtonB
{
    uint8_t         _pin; // дискретный вход

    uint8_t         on_time;
    uint8_t         off_time;

    ButtonPressType press;

    explicit ButtonB(uint8_t pin)
        : _pin(pin), on_time(0), off_time(0), press(ButtonPressType::NONE)
    {
        DDRB &= ~_BV(_pin);    // Input
        PORTB &= ~_BV(_pin);   // Убедиться что pull-up выключен
    }

    inline void reset()
    {
        press = ButtonPressType::NONE;
    }

    // Проверка нажатия кнопки
    bool pressed(CounterEvent event)
    {
        // Пока не обработано прошлое нажатие - кнопку не проверяем
        if (press != ButtonPressType::NONE)
        {
            return true;
        }

        if (bit_is_set(PINB, _pin) == LOW)
        {
            // Кнопка нажата
            if (on_time == 0)
            {
                // Начало нажатия
                on_time = 1;
                off_time = 0;
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
                press = ButtonPressType::LONG;
            }
            else if (on_time > 10)
            {
                press = ButtonPressType::SHORT;
            }
            on_time = off_time = 0;
        }

        return press != ButtonPressType::NONE;
    }
};
#endif

#if WATERIUS_MODEL == WATERIUS_MODEL_2

struct ButtonB2
{
    uint8_t         _pin; // дискретный вход

    ButtonPressType press;

    explicit ButtonB2(uint8_t pin)
        : _pin(pin), press(ButtonPressType::NONE)
    {
        DDRB &= ~_BV(_pin);    // Input
        PORTB &= ~_BV(_pin);   // Убедиться что pull-up выключен
    }

    inline void reset()
    {
        press = ButtonPressType::NONE;
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

#endif