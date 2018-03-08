
#include <Arduino.h>

#include <avr/sleep.h>
#include <avr/power.h>    
#include <avr/wdt.h>
#include <EdgeDebounceLite.h>
#include "sleep_counter.h"

EdgeDebounceLite debounce;

volatile uint8_t wdt_count; //0-60
volatile uint16_t btnCount;
volatile uint16_t btn2Count;

/* Watchdog interrupt vector */
ISR( WDT_vect ) { 
	wdt_count--;
}  

/* External interrupt */
ISR(PCINT0_vect)
{
	btnCount += (debounce.pin(BUTTON_PIN) == LOW);
#ifdef BUTTON2_PIN
	btn2Count += (debounce.pin(BUTTON2_PIN) == LOW);
#endif
}

/* Makes a deep sleep for X second using as little power as possible */
void gotoDeepSleep( uint16_t minutes, uint16_t *counter, uint16_t *counter2) 
{
	btnCount = *counter;
	btn2Count = *counter2;


	pinMode(BUTTON_PIN, INPUT_PULLUP);
#ifdef BUTTON2_PIN
	pinMode(BUTTON2_PIN, INPUT_PULLUP);
#endif

	power_all_disable();  // power off ADC, Timer 0 and 1, serial interface

	set_sleep_mode( SLEEP_MODE_PWR_DOWN );
	noInterrupts();       // timed sequence coming up
	resetWatchdog();      // get watchdog ready

	//wdt_enable(WDTO_1S);

	GIMSK |= (1 << PCIE);   // pin change interrupt enable
	PCMSK |= BUTTON_INTERRUPT; // pin change interrupt enabled for PCINTx

	interrupts();         // interrupts are required now
	
	for (uint16_t i = 0; i < minutes; ++i)
	{
		wdt_count = 60;
		while ( wdt_count > 0 ) 
		{
			sleep_mode();
		}
	}
		
	wdt_disable();  // disable watchdog
	//noInterrupts();       // timed sequence coming up

	PCMSK &= ~BUTTON_INTERRUPT;   // Turn off PBx as interrupt pin
	//GIMSK &= ~(1 << PCIE);   // pin change interrupt enable

	power_all_enable();   // power everything back on

	*counter = btnCount;
	*counter2 = btn2Count;

	pinMode(BUTTON_PIN, OUTPUT);  //save power
#ifdef BUTTON2_PIN
	pinMode(BUTTON2_PIN, OUTPUT);
#endif
}

/* Prepare watchdog */
void resetWatchdog() 
{
	MCUSR = 0; // clear various "reset" flags
	WDTCR = bit( WDCE ) | bit( WDE ) | bit( WDIF ); // allow changes, disable reset, clear existing interrupt
	// set interrupt mode and an interval (WDE must be changed from 1 to 0 here)
	WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // set WDIE, and 8 seconds delay
														
	wdt_reset(); // pat the dog
} 
