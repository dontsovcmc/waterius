#ifndef _COUNTER_h
#define _COUNTER_h

#include <Arduino.h>
#include <avr/wdt.h>

// значения компаратора с pull-up
//    : замкнут (0 ом) - намур-замкнут (1к5) - намур-разомкнут (5к5) - обрыв линии
// 0  : ?                ?                     ?                       ?
// если на входе 3к3
// 3к3:  100-108 - 140-142  - 230 - 1000
// 

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

struct CounterB 
{
    int8_t _checks; // -1 <= _checks <= TRIES
    
    uint8_t _pin;   // дискретный вход
    uint8_t _apin;  // номер аналогового входа

    uint16_t adc;   // уровень замкнутого входа
    uint8_t  state; // состояние входа

    explicit CounterB(uint8_t pin, uint8_t apin = 0)  
      : _checks(-1)
      , _pin(pin)
      , _apin(apin)
      , adc(0)
      , state(CounterState_e::CLOSE)
    {
       DDRB &= ~_BV(pin);      // INPUT
    }

    inline uint16_t aRead() 
    {
        PORTB |= _BV(_pin);      // INPUT_PULLUP
        analogRead(_apin);   // Switch MUX to channel and discard the read
        delayMicroseconds(1000);    // Wait for ADC S/H capacitor to charge
        uint16_t ret = analogRead(_apin); // Read ADC
        PORTB &= ~_BV(_pin);     // INPUT
        return ret;
    }

    bool is_close(uint16_t a)
    {
        state = value2state(a);
        return state == CounterState_e::CLOSE || state == CounterState_e::NAMUR_CLOSE;
    }

    bool is_impuls()
    { 
        //Детектируем импульс когда он заканчивается!
        //По сути софтовая проверка дребега

        uint16_t a = aRead();
        if (is_close(a)) {
            _checks = TRIES;
            adc = a;
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

struct ButtonB 
{
    uint8_t _pin;   // дискретный вход

    explicit ButtonB(uint8_t pin)  
      : _pin(pin)
    {
        DDRB &= ~_BV(pin);      // INPUT
        PORTB &= ~_BV(_pin);    // INPUT
    }

    inline bool digBit() 
    {
        return bit_is_set(PINB, _pin);
    }

    // Проверка нажатия кнопки 
    bool pressed() {

        if (digBit() == LOW)
        {	//защита от дребезга
            delayMicroseconds(20000);  //нельзя delay, т.к. power_off
            return digBit() == LOW;
        }
        return false;
    }

    // Замеряем сколько времени нажата кнопка в мс
    unsigned long wait_release() {

        unsigned long press_time = millis();
        while(pressed()) {
            wdt_reset();  
        }
        return millis() - press_time;
    }
};


#endif
