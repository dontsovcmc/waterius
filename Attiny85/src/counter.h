#ifndef _COUNTER_h
#define _COUNTER_h

#include <Arduino.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include "Power.h"


#if WATERIUS_MODEL == MODEL_CLASSIC 

    // значения компаратора с pull-up
    //    : замкнут (0 ом) - намур-замкнут (1к5) - намур-разомкнут (5к5) - обрыв линии
    // 0  : ?                ?                     ?                       ?
    // если на входе 3к3, подтяжка значит 33кОм
    // 3к3:  100-108 - 140-142  - 230 - 1000

    #define LIMIT_CLOSED 115       // < 115 - замыкание
    #define LIMIT_NAMUR_CLOSED 170 // < 170 - намур замкнут
    #define LIMIT_NAMUR_OPEN 800   // < 800 - намур разомкнут
                                // > - обрыв

#endif
#if WATERIUS_MODEL == MODEL_MINI 

    // значения компаратора с pull-up
    // : замкнут (0 ом) - намур-замкнут (1к5) - намур-разомкнут (5к5) - обрыв линии
    // на входе 1кОм, подтяжка значит 33кОм
    // : 30  - 72 - 170 - 1000

    #define LIMIT_CLOSED 50       // < 50 - замыкание
    #define LIMIT_NAMUR_CLOSED 150 // < 150 - намур замкнут
    #define LIMIT_NAMUR_OPEN 800   // < 800 - намур разомкнут
                                // > - обрыв
#endif


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
    ELECTRONIC,
    HALL,
    NONE = 0xFF
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
    uint8_t         _power;     // питание датчика
    uint8_t         _apin;      // номер аналогового входа

    uint8_t         on_time;    // время в замкнутом состоянии
    uint8_t         off_time;   // время в разомкнутом состоянии
    bool            active;     // идет потребление через счетчик

    uint16_t        adc;        // уровень замкнутого входа
    CounterState    state;      // состояние входа
    CounterType     type;       // тип выхода счетчика

    explicit CounterB(uint8_t pin, uint8_t apin = 0, uint8_t power = (uint8_t)-1)
        : _pin(pin), _power(power), _apin(apin), on_time(0), off_time(0), adc(0), state(CounterState::CLOSE), type(CounterType::NAMUR)
    {
        set_type(type);
    }

    void set_type(CounterType new_type)
    {
        if ((type == CounterType::HALL) && (_power != (uint8_t)-1)) {
            DDRB &= ~_BV(_power);       // Пин питания датчика возвращаем на вход
        }
        type = new_type;
        if ((type == CounterType::HALL) && (_power == (uint8_t)-1))
        {
            type = CounterType::NONE;
        }
        switch (type)
        {
            case CounterType::ELECTRONIC:
                PORTB |= _BV(_pin);                 // Включить pull-up
		        PCMSK |= _BV(_pin);                 // Включем прерывание по фронту
            case CounterType::DISCRETE:
            case CounterType::NAMUR:
                DDRB &= ~_BV(_pin);                 // Вход
                break;
            case CounterType::HALL:
                DDRB &= ~_BV(_pin);                 // Вход
                DDRB |= _BV(_power);                // Пин питания датчика ставим на выход
                PORTB &= ~_BV(_pin);                // Отключить pull-up
                PORTB |= _BV(_power);               // Включить питание
                delayMicroseconds(30);              // Задержка на включение датчика
                state = bit_is_set(PINB, _pin) ? CounterState::OPEN : CounterState::CLOSE;
                active = false;
                break;
            case CounterType::NONE:
                break;
        }
    }

    inline uint16_t aRead()
    {
	    power_adc_enable();
	    adc_enable();
        return analogRead(_apin);
    }

    bool electronic(CounterEvent event = CounterEvent::NONE)
    {
        state = bit_is_set(PINB, _pin) ? CounterState::OPEN : CounterState::CLOSE;
        if (state == CounterState::CLOSE)
        {
            // Замкнут
            if (on_time == 0)
            {
                // Начало импульса
                on_time = 1;
                off_time = 0;
                adc = aRead();
                return true;
            }
        }
        else
        {
            // Разомкнут
            on_time = 0;
        }
        return false;
    }

    bool hall(CounterEvent event = CounterEvent::NONE)
    {
        PORTB |= _BV(_power);               // Включить питание
        delayMicroseconds(30);              // Задержка на включение датчика
        CounterState new_state = bit_is_set(PINB, _pin) ? CounterState::OPEN : CounterState::CLOSE;
        if (new_state != state) 
        {
            state = new_state;
            if (!active) 
            {
                active = true;
                PCMSK |= _BV(_pin);         // Включем прерывание по фронту
            }
        }
        else if (!active)
        {
            PCMSK &= ~_BV(_pin);            // Отключем прерывание по фронту
            PORTB &= ~_BV(_power);          // Отключить питание
        }
        if (state == CounterState::CLOSE)
        {
            // Замкнут
            off_time = 0;
            if (on_time == 0)
            {
                // Начало импульса
                on_time = 1;
                adc = aRead();
                return true;
            }
            else
            {
                // Продолжение
                if (on_time < 200)
                {
                    // Увеличиваем счетчик времени в замкнутом состоянии
                    on_time += event == CounterEvent::TIME ? 10 : 1;
                } 
                else
                {
                    // Через 5 секунд отсутствия импульсов считаем что потребление закончилось
                    active = false;
                }
            }
        }
        else
        {
            // Разомкнут
            on_time = 0;
            // Увеличиваем счетчик времени в разомкнутом состоянии
            if (off_time < 200)
            {
                off_time += event == CounterEvent::TIME ? 10 : 1;
            }
            else
            {
                // Через 5 секунд отсутствия импульсов считаем что потребление закончилось
                active = false;
            }
        }
        return false;
    }

    bool discrete(CounterEvent event = CounterEvent::NONE)
    {
        PORTB |= _BV(_pin);                 // Включить pull-up
        delayMicroseconds(30);
        if (type == CounterType::NAMUR)
        {
            uint16_t a = aRead();
            state = value2state(a);
            if (state <= CounterState::NAMUR_CLOSE)
            {
                adc = a;
            }
        }
        else
        {
            state = bit_is_set(PINB, _pin) ? CounterState::OPEN : CounterState::CLOSE;
            if ((state == CounterState::CLOSE) && (on_time == 0))
            {
                adc = aRead();
            }
        }
        PORTB &= ~_BV(_pin);                // Отключить pull-up

        if (state == CounterState::CLOSE || state == CounterState::NAMUR_CLOSE)
        {
            // Замкнут
            off_time = 0;
            if (on_time == 0)
            {
                // Начало импульса
                on_time = 1;
                return true;
            }
            else
            {
                // Продолжение
                if (on_time < 200)
                {
                    // Увеличиваем счетчик времени в замкнутом состоянии
                    on_time += event == CounterEvent::TIME ? 10 : 1;
                } 
            }
        }
        else
        {
            // Разомкнут
            if (on_time > 0)
            {
                // Идет обработка импульса
                if (off_time < 20)
                {
                    // Увеличиваем счетчик времени в разомкнутом состоянии
                    off_time += event == CounterEvent::TIME ? 10 : 1;
                }
                else
                {
                    // Ждем 750мс после конца импульса и завершаем его
                    on_time = 0;
                }
            }
        }
        return false;
    }

    bool is_impuls(CounterEvent event = CounterEvent::NONE)
    {
        switch (type) 
        {
            case CounterType::ELECTRONIC:
                return electronic(event);
            case CounterType::HALL:
                return hall(event);
            case CounterType::NAMUR:
            case CounterType::DISCRETE:
                return discrete(event);
            case CounterType::NONE:
            default:
                return false;
        }
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