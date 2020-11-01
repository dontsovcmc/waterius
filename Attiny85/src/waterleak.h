#ifndef _WATERLEAK_h
#define _WATERLEAK_h

#include <Arduino.h>
#include "Setup.h"

#ifdef WATERIUS_4C2W

enum WaterLeak_e
{
    OFF,        //не установлен. определяется при включении.
    BREAK,      //обрыв линии, нет датчика
    NORMAL,     //норма
    WATER       //вода
};

struct LeakPowerB
{
    uint8_t _pin;

    LeakPowerB(uint8_t pin) 
    : _pin(pin)
    {
        DDRB &= ~_BV(pin);      // INPUT
    }

    void power(bool on)
    {
        DDRB |= _BV(_pin);         // set pin of Port B as output

        if (on) {
            PORTB |= _BV(_pin);    // set pin 3 of Port B high
        } else {
            PORTB &= ~_BV(_pin);   // set pin 3 of Port B low
            DDRB &= ~_BV(_pin);     // INPUT
        } 
    }
};

// Датчик протечки 36кОм
// 3x4 см лабиринт дорожек.
// 
// Норма 162-170
// Вода  127 (полностью погружен 20кОм) - 152 (мало погружен 32кОм)
// Короткое замыкание 15

// 91 кОм - 322

// BREAK - обрыв
#define LIMIT_NORMAL  500
// NORMAL - ок
#define LIMIT_WATER   150
// WATER - вода

/*
Вход датчика протечки

Пин подтянут к + через 200кОм
Сопротивление датчика протечки 36кОм
Между датчиком и GND 3.3кОм
*/
struct WaterLeakA
{
    int8_t _checks; // -1 <= _checks <= TRIES
    
    uint8_t _pin;   // дискретный вход
    uint8_t _apin;  // номер аналогового входа

    uint16_t adc;   // уровень входа
    uint8_t  state; // состояние входа

    explicit WaterLeakA(uint8_t pin, uint8_t apin = 0)  
      : _checks(-1)
      , _pin(pin)
      , _apin(apin)
      , adc(0)
      , state(WaterLeak_e::OFF)
    {
       DDRA &= ~_BV(pin);      // INPUT
    }
    
    /* Не забываем включить питание компаратора перед чтение входа */
    inline uint16_t aRead() 
    {
        uint16_t ret = analogRead(_apin); // в TinyCore там работа с битами, оптимально
        return ret;
    }

    void update()  //TODO external
    {
        adc = aRead();
        state = value2state(adc);
    }

    inline bool is_ok() 
    {
        return !is_work() || state == WaterLeak_e::NORMAL;
    }

    // Возвращаем текущее состояние входа
    inline enum WaterLeak_e value2state(uint16_t value)   //TODO external
    {
        if (value < LIMIT_WATER) {
            return WaterLeak_e::WATER;
        } else if (value < LIMIT_NORMAL) {
            return WaterLeak_e::NORMAL;
        } else {
            return WaterLeak_e::BREAK;
        }
    }
    
    // Если в режиме настройки нет датчика, то считаем его выключеным
    void turn_off_if_break()
    {
        LOG(F("turn_off_if_break (1): "));
        LOG(state);
        if (state == WaterLeak_e::BREAK) {
            state = WaterLeak_e::OFF;
        }
    }

    void set_work(bool is_work) 
    {
        if (is_work) {
            state = WaterLeak_e::NORMAL;
        } else {
            state = WaterLeak_e::OFF;
        }
    }

    inline bool is_work()
    {
        if (state == WaterLeak_e::OFF) {
            return false;
        } else {
            return true;
        }
    }
};

#endif  //WATERIUS_4C2W т.к. PORTA DDRA нет в attiny85

#endif