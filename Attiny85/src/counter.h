#ifndef _COUNTER_h
#define _COUNTER_h

#include <Arduino.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include "Power.h"

// значения компаратора с pull-up
//    : замкнут (0 ом) - намур-замкнут (1к5) - намур-разомкнут (5к5) - обрыв линии
// 0  : ?                ?                     ?                       ?
// если на входе 3к3
// 3к3:  100-108 - 140-142  - 230 - 1000
//

#define LIMIT_CLOSED 115       // < 115 - замыкание
#define LIMIT_NAMUR_CLOSED 170 // < 170 - намур замкнут
#define LIMIT_NAMUR_OPEN 800   // < 800 - намур разомкнут
                               // > - обрыв

enum CounterState
{
    CLOSE,
    NAMUR_CLOSE,
    NAMUR_OPEN,
    OPEN
};

enum CounterType 
{
    NAMUR,
    DISCRETE,
    ELECTRONIC
};

enum class CounterEvent
{
    NONE,
    FRONT,
    TIME,
};

struct CounterB
{
    uint8_t         _pin;       // дискретный вход
    uint8_t         _apin;      // номер аналогового входа

    uint8_t         on_time;
    uint8_t         off_time;

    uint16_t        adc;        // уровень замкнутого входа
    CounterState    state;      // состояние входа
    CounterType     type;       // тип выхода счетчика

    explicit CounterB(uint8_t pin, uint8_t apin = 0)
        : _pin(pin), _apin(apin), on_time(0), off_time(0), adc(0), state(CounterState::CLOSE), type(CounterType::NAMUR)
    {
        DDRB &= ~_BV(pin);      // Input
    }

    inline uint16_t aRead()
    {
	    power_adc_enable();
	    adc_enable();
        return analogRead(_apin);
    }

    bool is_close()
    {
        switch (type) 
        {
            case CounterType::DISCRETE:
                PORTB |= _BV(_pin);               // Включить pull-up
                delayMicroseconds(30);
                state = bit_is_set(PINB, _pin) ? CounterState::OPEN : CounterState::CLOSE;
                if ((state == CounterState::CLOSE) && (on_time == 0))
                {
                    adc = aRead();
                }
                PORTB &= ~_BV(_pin);              // Отключить pull-up
                break;
            case CounterType::ELECTRONIC:
                PORTB |= _BV(_pin);               // Включить pull-up
                state = bit_is_set(PINB, _pin) ? CounterState::OPEN : CounterState::CLOSE;
                if ((state == CounterState::CLOSE) && (on_time == 0))
                {
                    adc = aRead();
                }
                break;
            default:
                PORTB |= _BV(_pin);               // Включить pull-up
                uint16_t a = aRead();
                PORTB &= ~_BV(_pin);              // Отключить pull-up
                state = value2state(a);
                if (state <= CounterState::NAMUR_CLOSE)
                {
                    adc = a;
                }
                break;
        }
        return state == CounterState::CLOSE || state == CounterState::NAMUR_CLOSE;
    }

    bool is_impuls(CounterEvent event = CounterEvent::NONE)
    {
        if (is_close())
        {
            // Замкнут
            if (on_time == 0)
            {
                // Начало импульса
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
            // Разомкнут
            if (on_time > 0)
            {
                // Идет обработка импульса
                if (off_time < 200)
                {
                    // Увеличиваем счетчик времени в разомкнутом состоянии
                    off_time += event == CounterEvent::TIME ? 10 : 1;
                }
            }
        }

        // Определяем конец импульса
        bool result = false;
        if (on_time > 0)
        {
            switch (type)
            {
                case CounterType::DISCRETE:
                case CounterType::NAMUR:
                    // Ждем 750мс после конца импульса и завершаем его
                    if (off_time > 20)
                    {
                        if (on_time >= 20) 
                        {
                            // Уведомляем о импульсе в его конце если была достаточная длительность
                            result = true;
                        }
                        on_time = 0;
                    }
                    break;
                case CounterType::ELECTRONIC:
                    // Уведомляем о импульсе в его конце
                    result = true;
                    // У электронного счетчика транзисторный выход и дребезга нет, сразу завершаем импульс
                    if (off_time > 0)
                    {
                        on_time = 0;
                    }
                    break;
            }
        }

        // Уведомляем о импульсе
        return result;
    }

    // Возвращаем текущее состояние входа
    inline CounterState value2state(uint16_t value)
    {
        if (value < LIMIT_CLOSED)
        {
            return CounterState::CLOSE;
        }
        else if (value < LIMIT_NAMUR_CLOSED)
        {
            return CounterState::NAMUR_CLOSE;
        }
        else if (value < LIMIT_NAMUR_OPEN)
        {
            return CounterState::NAMUR_OPEN;
        }
        else
        {
            return CounterState::OPEN;
        }
    }
};

#endif