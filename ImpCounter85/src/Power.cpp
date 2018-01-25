#include "Power.h"
#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>    
#include <avr/wdt.h>
#include <EdgeDebounceLite.h>

EdgeDebounceLite debounce;

volatile bool wdtInterrupt;    //watchdog timer interrupt flag
volatile uint32_t btnCount;
volatile uint32_t btn2Count;


//Disabling ADC saves ~230uAF. Needs to be re-enable for the internal voltage check
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC


/* Watchdog interrupt vector */
ISR( WDT_vect ) { 
	wdtInterrupt = true;
}  

ISR(PCINT0_vect)
{
	btnCount += (debounce.pin(BUTTON_PIN) == LOW);
//#ifndef DEBUG
//	btn2Count += (debounce.pin(BUTTON2_PIN) == LOW);
//#endif
}

/* Makes a deep sleep for X second using as little power as possible */
void gotoDeepSleep( uint32_t seconds, uint32_t *counter, uint32_t *counter2) 
{
	btnCount = *counter;
	btn2Count = *counter2;

	pinMode( 0, INPUT );
	pinMode( 2, INPUT );
//#ifndef DEBUG
//	pinMode(3, INPUT_PULLUP);
//#endif
	pinMode(4, INPUT_PULLUP);

	wdtInterrupt = false;

	while ( seconds > 0 ) {
		set_sleep_mode( SLEEP_MODE_PWR_DOWN );
		adc_disable();            // turn off ADC
		power_all_disable();  // power off ADC, Timer 0 and 1, serial interface
		noInterrupts();       // timed sequence coming up
		resetWatchdog();      // get watchdog ready
		GIMSK |= (1 << PCIE);   // pin change interrupt enable

//#ifndef DEBUG		
		//PCMSK |= (1 << PCINT4 | 1 << PCINT3); // pin change interrupt enabled for PCINT4
//#else
		PCMSK |= (1 << PCINT4); // pin change interrupt enabled for PCINT4
//#endif
		sleep_enable();       // ready to sleep
		interrupts();         // interrupts are required now

		sleep_cpu();          // sleep                
		sleep_disable();      // precaution
		
		wdt_disable();  // disable watchdog
		
		noInterrupts();       // timed sequence coming up
///#ifndef DEBUG		
		//PCMSK &= ~_BV(PCINT4 | PCINT3);   // Turn off PB3 as interrupt pin
//#else
		PCMSK &= ~_BV(PCINT4);   // Turn off PB3 as interrupt pin
//#endif		
		if (wdtInterrupt) {
			wdtInterrupt = false;
			seconds--;
		}
	}
	power_all_enable();   // power everything back on

	*counter = btnCount;
	*counter2 = btn2Count;
}

/* Prepare watchdog */
void resetWatchdog() {
	MCUSR = 0; // clear various "reset" flags
	WDTCR = bit( WDCE ) | bit( WDE ) | bit( WDIF ); // allow changes, disable reset, clear existing interrupt
	// set interrupt mode and an interval (WDE must be changed from 1 to 0 here)
	WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // set WDIE, and 8 seconds delay
														
	wdt_reset(); // pat the dog
} 

/* Remove Wakes up the ESP by pulling it's reset pin low for a short time  */
void wakeESP() {
	pinMode( ESP_RESET_PIN, OUTPUT );
	digitalWrite( ESP_RESET_PIN, LOW );
	delay( 10 );
	digitalWrite( ESP_RESET_PIN, HIGH );
	pinMode( ESP_RESET_PIN, INPUT_PULLUP );
}

uint16_t readVcc() 
{
	// Read 1.1V reference against AVcc
	// set the reference to Vcc and the measurement to the internal 1.1V reference

	//Re-enable ADC 
	adc_enable();

	#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
	#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
		ADMUX = _BV(MUX3) | _BV(MUX2);
	#endif 

	delay(2); // Wait for Vref to settle
	ADCSRA |= _BV(ADSC); // Start conversion
	while (bit_is_set(ADCSRA,ADSC)); // measuring

	uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH 
	uint8_t high = ADCH; // unlocks both

	uint16_t result = (high<<8) | low;

	//result = 1126400L / result; // calculate Vcc (in mV); 1126400 = 1.1*1024*1000 // generally
	result = 1126400L / result; // calculate Vcc (in mV); 1168538 = 1.14115*1024*1000 // for this ATtiny

	//Disable ADC
	adc_disable();

	return result; // Vcc in millivolts
}