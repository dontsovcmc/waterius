#ifndef _POWERSAVE_h
#define _POWERSAVE_h

#include <Arduino.h>

//Выключение ADC сохраняет ~230uAF. 
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

void wakeESP();
uint16_t readVcc();

#endif

