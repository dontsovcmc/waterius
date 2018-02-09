#ifndef _POWERSAVE_h
#define _POWERSAVE_h

#include <Arduino.h>

//Disabling ADC saves ~230uAF. Needs to be re-enable for the internal voltage check
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

void wakeESP();
uint16_t readVcc();

#endif

