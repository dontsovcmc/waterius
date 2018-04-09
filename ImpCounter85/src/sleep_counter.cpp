
#include <Arduino.h>

#include <avr/sleep.h>
#include <avr/power.h>    
#include <avr/wdt.h>
#include "sleep_counter.h"

volatile uint16_t wdt_count; 

/* Watchdog interrupt vector */
ISR( WDT_vect ) { 
	wdt_count--;
}  

/* Makes a deep sleep for X second using as little power as possible */
// 65535 ~ 18 hours
void gotoDeepSleep(uint16_t minutes, Counter *counter, Counter *counter2) 
{

	power_all_disable();  // power off ADC, Timer 0 and 1, serial interface

	set_sleep_mode( SLEEP_MODE_PWR_DOWN );

	noInterrupts();       // timed sequence coming up
	resetWatchdog();      // get watchdog ready
	//wdt_enable(WDTO_1S);
	interrupts();         // interrupts are required now
	
	for (uint16_t i = 0; i < 60 * 4; ++i)  //4 т.к. 250мс сон
	{
		wdt_count = minutes; 
		while ( wdt_count > 0 ) 
		{
			counter->check();
			#ifdef BUTTON2_PIN
				counter2->check();
			#endif

			sleep_mode();
		}
	}
		
	wdt_disable();        // disable watchdog
	//noInterrupts();       // timed sequence coming up
	power_all_enable();   // power everything back on
}

/* Prepare watchdog */
void resetWatchdog() 
{
	MCUSR = 0; // clear various "reset" flags
	WDTCR = bit( WDCE ) | bit( WDE ) | bit( WDIF ); // allow changes, disable reset, clear existing interrupt
	// set interrupt mode and an interval (WDE must be changed from 1 to 0 here)
	WDTCR = bit( WDIE ) | bit( WDP2 );    // set WDIE, and 0.25 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP0 );    // set WDIE, and 0.5 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // set WDIE, and 8 seconds delay
														
	wdt_reset(); // pat the dog
} 


Counter::Counter()
	: i(0)
	, state(false)
{
	debounce.setSensitivity(4);
}

void Counter::check()
{
	pinMode(pin, INPUT_PULLUP);

	if (debounce.pin(pin) == LOW)
	{
		if (state == false)
		{
			delayMicroseconds(20000); //delay failed (?)
			if (debounce.pin(pin) == LOW)
			{
				i++;
				state = true;
			}
		}
	}
	else
	{
		state = false;
	}

	pinMode(pin, INPUT);
}
