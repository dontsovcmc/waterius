#ifndef _COUNTER_h
#define _COUNTER_h

#include <Arduino.h>

#define OPENED       0  //разомкнут
#define FIRST_CHECK  1  //1й раз прочитал замыкание
#define CLOSED       2  //2й раз замыкание - значит точно замкнут

struct Counter 
{
	uint8_t state;  
    uint8_t pin;

    Counter(const uint8_t p)
        : state(OPENED)
        , pin(p)
    { }

    bool check_close(uint32_t &i)
    {
        pinMode(pin, INPUT_PULLUP); //DDRB &= ~_BV(pin);  PORTB |= _BV(pin); //http://microsin.net/programming/avr/accessing-avr-ports-with-winavr-gcc.html

        if (digitalRead(pin) == LOW)  //bit_is_clear(PINB, pin) 
        {
            if (state == OPENED)
            {
                state = FIRST_CHECK;
            }
            else if (state == FIRST_CHECK)
            {
                i++;
                state = CLOSED;
                pinMode(pin, INPUT);  //DDRB &= ~_BV(pin);  PORTB &= ~_BV(pin);
                return true;
            }
        }
        else
        {
            state = OPENED;
        }

        pinMode(pin, INPUT);  //DDRB &= ~_BV(pin);  PORTB &= ~_BV(pin);
        return false;
    }

};


#endif