#ifndef _COUNTER_h
#define _COUNTER_h

#include <Arduino.h>

#define OPENED       0  //разомкнут
#define FIRST_CHECK  1  //1й раз прочитал замыкание
#define CLOSED       2  //2й раз замыкание - значит точно замкнут

struct Counter 
{
	uint16_t i;

	uint8_t state;  
  
    uint8_t pin;

    Counter(const uint8_t p)
        : i(0)
        , state(OPENED)
        , pin(p)
    { }

    bool check_close()
    {
        pinMode(pin, INPUT_PULLUP);

        if (digitalRead(pin) == LOW)
        {
            if (state == OPENED)
            {
                state = FIRST_CHECK;
            }
            else if (state == FIRST_CHECK)
            {
                i++;
                state = CLOSED;
            }
        }
        else
        {
            state = OPENED;
        }

        pinMode(pin, INPUT);
    }
};


#endif