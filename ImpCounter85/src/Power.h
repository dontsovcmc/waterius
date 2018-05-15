#ifndef _POWERSAVE_h
#define _POWERSAVE_h

#include <Arduino.h>

//Выключение ADC сохраняет ~230uAF. 
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

uint16_t readVcc();

struct ESPPowerButton {
	
	ESPPowerButton(const uint8_t);
	uint8_t pin;

	bool pressed;
	bool power_on;
	unsigned long power_timestamp;

	void check();
	void power(const bool);
};



int freeRam();
#endif

