#ifndef _WBUTTON_h
#define _WBUTTON_h

#include "Arduino.h"

#define LONG_PRESS_SEC 3

struct WateriusButton
{
    uint8_t _pin;

    WateriusButton(uint8_t pin)
        : _pin(pin)
    {
        pinMode(_pin, INPUT);
    }
    

    // Проверка нажатия кнопки 
    bool button_pressed() 
    {
        if (digitalRead(_pin) == LOW)
        {	//защита от дребезга
            delayMicroseconds(20000);  
            return digitalRead(_pin) == LOW;
        }
        return false;
    }

    // Замеряем сколько времени нажата кнопка в мс
    unsigned long wait_button_release() 
    {
        unsigned long press_time = millis();
        while(button_pressed())
            ;  
        return millis() - press_time;
    }

    bool long_press() 
    {
        return wait_button_release() > LONG_PRESS_SEC * 1000; 
    }
};

#endif