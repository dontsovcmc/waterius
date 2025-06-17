
#include "Power.h"
#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "Setup.h"

ESPPowerPin::ESPPowerPin(const uint8_t p)
    : power_pin(p), wake_up_timestamp(0)
{
    pinMode(power_pin, INPUT);
}

void ESPPowerPin::power(const bool on)
{
    if (on)
    {
        pinMode(power_pin, OUTPUT);

        digitalWrite(power_pin, HIGH);
        wake_up_timestamp = millis();
    }
    else
    {
        digitalWrite(power_pin, LOW);

        pinMode(power_pin, INPUT); // снижаем потребление
        wake_up_timestamp = 0;
    }
}

bool ESPPowerPin::elapsed(const unsigned long msec)
{
    return millis() - wake_up_timestamp > msec;
}

void ESPPowerPin::extend_wake_up()
{
    wake_up_timestamp = millis();
}


// Измеряем напряжение питания Attiny85. 
// Есть погрешность, поэтому надо калбировать. Для каждой attiny будет своя константа 1126400L.
// Мы добавим калибровку в ESP, т.к. невозможно хранить константу в attiny без изменения Header и версии =(
// https://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/
uint16_t readVcc(uint16_t vcc_reference) 
{
    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference

    // Включаем ADC 
    power_adc_enable();
    adc_enable();

    ADMUX = _BV(MUX3) | _BV(MUX2);   // attiny85
    
    //delayMicroseconds(5);    // only needed if using sensors with source resistance > 10K
    delay(2); // Wait for Vref to settle. See Table 17-4. Input Channel Selections
    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA,ADSC)); // measuring

    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH 
    uint8_t high = ADCH; // unlocks both

    uint16_t adc_result = (high<<8) | low;

    //result = 1126400L / result; // calculate Vcc (in mV); 1126400 = 1.1*1024*1000 // generally
    adc_result = (uint32_t)vcc_reference * 1024L / adc_result;

    //Выключаем ADC
    adc_disable();
    power_adc_disable();

    return adc_result; //миливольт
}
