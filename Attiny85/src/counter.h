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

#define TRIES_CLOSE 2   //Минимальная длительность состояния "1"
#define TRIES_OPEN  24  //Минимальная длительность состояния "0"

enum CounterState_e
{
    CLOSE,
    NAMUR_CLOSE,
    NAMUR_OPEN,
    OPEN
};

struct CounterB
{
    uint8_t _pin;   // дискретный вход
    uint8_t _apin;  // номер аналогового входа

    uint16_t closed_adc;   // уровень замкнутого входа

    uint8_t  stable_state; // стабильное состояние входа
    uint8_t  temp_state; // нестабильное состояние входа
    int8_t _checks; // -1 <= _checks <= TRIES

    explicit CounterB(uint8_t pin, uint8_t apin = 0)
      : _pin(pin)
      , _apin(apin)
      , closed_adc(0)
      , stable_state(CounterState_e::OPEN)
      , temp_state(CounterState_e::OPEN)
      , _checks(-1)
    {
       DDRB &= ~_BV(pin);      // INPUT
    }

    uint16_t aRead()
    {
        PORTB |= _BV(_pin);      // INPUT_PULLUP
        analogRead(_apin);   // Switch MUX to channel and discard the read
        delayMicroseconds(1000);    // Wait for ADC S/H capacitor to charge
        uint16_t ret = analogRead(_apin); // Read ADC
        PORTB &= ~_BV(_pin);     // INPUT
        return ret;
    }

    bool is_close(uint8_t s)
    {
        return s == CounterState_e::CLOSE || s == CounterState_e::NAMUR_CLOSE;
    }

    inline void update_state(void)
    {
      bool old_state = is_close(temp_state);

      uint16_t cur_adc = aRead();
      temp_state = value2state(cur_adc);

      bool new_state = is_close(temp_state);

      if(old_state == new_state)
      {
        if(_checks >= 0)
        {
           _checks -= 1;
        }

        // Signal is stable now
        if(_checks == 0)
        {
           stable_state = temp_state;
           if(new_state)
           {
              closed_adc = cur_adc;
           }
        }
        return;
      }

      _checks = new_state ? TRIES_CLOSE : TRIES_OPEN;
    }

    bool is_impuls(void)
    {
        bool old_state = is_close(stable_state);
        update_state();
        bool new_state = is_close(stable_state);

        //Детектируем импульс когда он заканчивается!
        //По сути софтовая проверка дребега

        return old_state && !new_state;
    }

    // Возвращаем текущее состояние входа
    enum CounterState_e value2state(uint16_t value)
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
