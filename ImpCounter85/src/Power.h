#ifndef _POWERSAVE_h
#define _POWERSAVE_h

#include <Arduino.h>

//Выключение ADC сохраняет ~230uAF. 
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

uint16_t readVcc();

struct ESPPowerButton 
{
	ESPPowerButton(const uint8_t, const uint8_t);
	uint8_t power_pin;
	uint8_t setup_pin;

	bool pressed;                  //пользователь нажал кнопку
	bool power_on;  		       //Wi-Fi включен
	unsigned long wake_up_timestamp; //время включения Wi-Fi

	void check();
	bool is_pressed();
	void power(const bool);
};

#endif

