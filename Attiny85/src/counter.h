#ifndef _COUNTER_h
#define _COUNTER_h

#include <Arduino.h>

// значения изменятся, если на входе не будет резистора 3к3
//    : замкнут - намур-замкнут - намур-разомкнут - обрыв линии
// 3к3:  100 - 140 - 230 - 1000

#define LIMIT_CLOSED       115   // < 115 - замыкание 
#define LIMIT_NAMUR_CLOSED 170   // < 170 - намур замкнут
#define LIMIT_NAMUR_OPEN   800   // < 800 - намур разомкнут
                                 // > - обрыв

#define TRIES 3  //Сколько раз после окончания импульса 
                 //должно его не быть, чтобы мы зарегистририровали импульс

enum CounterState_e
{
    CLOSE,
    NAMUR_CLOSE,
    NAMUR_OPEN,
    OPEN
};

struct Counter 
{
    int8_t _checks; // -1 <= _checks <= TRIES
    
    uint8_t _pin;   // дискретный вход
    uint8_t _apin;  // номер аналогового входа

    uint8_t state; // состояние входа

    Counter(uint8_t pin, uint8_t apin = 0)  
      : _pin(pin)
      , _apin(apin)
    {
       DDRB &= ~_BV(pin);      // INPUT
    }

    inline uint16_t aRead() 
    {
        PORTB |= _BV(_pin);      // INPUT_PULLUP
        uint16_t ret = analogRead(_apin); // в TinyCore там работа с битами, оптимально
        PORTB &= ~_BV(_pin);     // INPUT
        return ret;
    }

    inline bool digBit() 
    {
        return bit_is_set(PINB, _pin);
    }

    inline bool digRead() 
    {
        PORTB |= _BV(_pin);  // INPUT_PULLUP
        bool ret = digBit();
        PORTB &= ~_BV(_pin); // INPUT
        return ret;
    }
    
    bool is_close()
    {
        //bool value = digRead();
        //state = value2state(value ? 1024 : 0);
        uint16_t value = aRead();
        state = value2state(value);
        return state == CounterState_e::CLOSE || state == CounterState_e::NAMUR_CLOSE;
    }

    bool is_impuls()
    { 
        //Детектируем импульс когда он заканчивается.
        //Дребезг проверяем 
        if (is_close()) {
            _checks = TRIES;
        }
        else {
            if (_checks >= 0) {
                _checks--;
            }
            if (_checks == 0) {
                return true;
            }
        }
        return false;
    }

    // Возвращаем текущее состояние входа
    inline enum CounterState_e value2state(uint16_t value) 
    {
        if (value < LIMIT_CLOSED) {
            return CounterState_e::CLOSE;
        } else if (value < LIMIT_NAMUR_CLOSED) {
            return CounterState_e::NAMUR_CLOSE;
        } else if (value < LIMIT_NAMUR_OPEN) {
            return CounterState_e::NAMUR_OPEN;
        } else {
            return CounterState_e::OPEN;
        }
    }
};

#endif