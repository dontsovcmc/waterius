

//#define USE_BUTTON2

#include <avr/sleep.h>
#include <avr/power.h>    
#include <avr/wdt.h>

#include <TinyDebugSerial.h>

#define BUTTON_PIN    2
#define BUTTON2_PIN   1

TinyDebugSerial mySerial;
static uint32_t counter = 0;
static uint32_t counter2 = 0;

static bool state = false;
static bool state2 = false;

#define DEBUG

#ifdef DEBUG
  #define DEBUG_CONNECT(x)  mySerial.begin(x)
  #define DEBUG_PRINT(x)    mySerial.print(x)
  #define DEBUG_PRINTLN(x)    mySerial.println(x)
  #define DEBUG_FLUSH()     mySerial.flush()
#else
  #define DEBUG_CONNECT(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x) 
  #define DEBUG_FLUSH()
#endif

#include <EdgeDebounceLite.h>

EdgeDebounceLite debounce;

volatile uint32_t wdt_count;

/* Watchdog interrupt vector */
ISR( WDT_vect ) { 
	wdt_count--;
}  


/* Prepare watchdog */
void resetWatchdog() 
{
	MCUSR = 0; // clear various "reset" flags
	WDTCR = bit( WDCE ) | bit( WDE ) | bit( WDIF ); // allow changes, disable reset, clear existing interrupt
	// set interrupt mode and an interval (WDE must be changed from 1 to 0 here)
	WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP0 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // set WDIE, and 8 seconds delay
														
	wdt_reset(); // pat the dog
} 

//without pull-up resistor. diodes for changes polarity
void checkPin2( uint8_t outPin, uint8_t inPin, uint32_t *counter, bool *state)
{
	pinMode(inPin, INPUT_PULLUP);
	pinMode(outPin, OUTPUT);
	digitalWrite(outPin, LOW);

	if (debounce.pin(inPin) == LOW)
    {
        if (*state == false)
        {
            *counter = *counter + 1;
            *state = true;
        }
    }
    else
    {
        *state = false;
    }

	pinMode(outPin, INPUT);
	pinMode(inPin, INPUT);
}

//classic
void checkPin( uint8_t outPin, uint8_t inPin, uint32_t *counter, bool *state)
{
	if (debounce.pin(inPin) == LOW)
    {
        if (*state == false)
        {
            *counter = *counter + 1;
            *state = true;
        }
    }
    else
    {
        *state = false;
    }
}

/* Makes a deep sleep for X second using as little power as possible */
void gotoDeepSleep( uint32_t seconds, uint32_t *counter, uint32_t *counter2, bool *state, bool *state2) 
{
	wdt_count = seconds * 2;  //0.5s wdt

	power_all_disable();  // power off ADC, Timer 0 and 1, serial interface

	set_sleep_mode( SLEEP_MODE_PWR_DOWN );

	noInterrupts();       // timed sequence coming up
	resetWatchdog();      // get watchdog ready
	interrupts();         // interrupts are required now

	pinMode(BUTTON2_PIN, INPUT);
	pinMode(BUTTON_PIN, INPUT);	

	while ( wdt_count > 0 ) 
	{
        checkPin(BUTTON_PIN, BUTTON2_PIN, counter2, state2);
		checkPin(BUTTON2_PIN, BUTTON_PIN, counter, state);
        
		sleep_mode();
	}

	wdt_disable();  // disable watchdog
	noInterrupts();       // timed sequence coming up
	
	power_all_enable();   // power everything back on
}

void setup() 
{
    // put your setup code here, to run once:
    resetWatchdog(); // Needed for deep sleep to succeed

    debounce.setSensitivity(4);

    DEBUG_CONNECT(9600);
  	DEBUG_PRINTLN(F("==== START ===="));

}

void loop() 
{
    // put your main code here, to run repeatedly:
    gotoDeepSleep(2, &counter, &counter2, &state, &state2);		// Deep sleep for X seconds

    DEBUG_PRINT("counter=");
    DEBUG_PRINT(counter);
    DEBUG_PRINT(", counter2=");
    DEBUG_PRINTLN(counter2);
}