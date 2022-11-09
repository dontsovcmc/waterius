#ifndef _WATERIUS_POWER_h
#define _WATERIUS_POWER_h

#include <Arduino.h>

//Выключение ADC сохраняет ~230uAF.
#define adc_disable() (ADCSRA &= ~(1 << ADEN)) // disable ADC
#define adc_enable() (ADCSRA |= (1 << ADEN))   // re-enable ADC

/*
    Включение питания ESP подачей HIGH на ESP_POWER_PIN пин.
    Класс нужен для измерения времени подачи питания.
    Из-за нехватки pin на Attiny85 мы сажаем кнопку на линию i2c.
*/
struct ESPPowerPin
{
    explicit ESPPowerPin(const uint8_t);

    //Пин подачи питания на ESP
    uint8_t power_pin;

    //Время включения Wi-Fi
    unsigned long wake_up_timestamp;

    //Подать или снять питание с ESP
    void power(const bool);

    //Прошло ли больше msec времени с момента wake_up_timestamp
    bool elapsed(const unsigned long msec);
};

#endif
