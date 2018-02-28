
#include "Setup.h"

#include <avr/pgmspace.h>
#include <TinyDebugSerial.h>
#include <USIWire.h>

#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"
#include "sleep_counter.h"

#ifdef DEBUG
	TinyDebugSerial mySerial;
#endif

static enum State state = SLEEP;
static unsigned long secondsSleeping = 0;
static unsigned long masterWokenUpAt;

static unsigned long counter = 0;
static unsigned long counter2 = 0;

struct SensorData {
	unsigned long counter;	     
	unsigned long counter2;     
};

//https://github.com/esp8266/Arduino/issues/1825
struct SlaveStats info = {
	0, //bytesReady
	WAKE_MASTER_EVERY,
	MEASUREMENT_EVERY,
	0, //vcc
	0,
	DEVICE_ID,
	NUMBER_OF_SENSORS,
	0 //dummy
};

Storage storage( sizeof( SensorData ) );
SlaveI2C slaveI2C;

void setup() 
{
	DEBUG_CONNECT(9600);
  	DEBUG_PRINTLN(F("==== START ===="));

	resetWatchdog(); //??? Needed for deep sleep to succeed
	adc_disable(); //Disable ADC

	//wake up when turn on
	secondsSleeping = WAKE_MASTER_EVERY;
	state = MEASURING;
}

void loop() 
{
  	DEBUG_PRINT(F("LOOP (")); DEBUG_PRINT(state); DEBUG_PRINTLN(F(")"));

	switch ( state ) {
		case SLEEP:
			slaveI2C.end();			// We don't want to be i2c slave anymore. 
			//delay(3000);
			gotoDeepSleep( MEASUREMENT_EVERY, &counter, &counter2);		// Deep sleep for X seconds
			secondsSleeping += MEASUREMENT_EVERY;	// Keep a track of how many seconds we have been sleeping
			state = MEASURING;

			DEBUG_PRINT(F("COUNTER=")); DEBUG_PRINT(counter);
			DEBUG_PRINT(F(" COUNTER2=")); DEBUG_PRINTLN(counter2);
			break;

		case MEASURING:
			SensorData sensorData;
			sensorData.counter = counter;
			sensorData.counter2 = counter2;
			storage.addElement( &sensorData );

			state = SLEEP;			// Sucessfully stored the data, starting sleeping again
			if ( secondsSleeping >= WAKE_MASTER_EVERY ) 
				state = MASTER_WAKE; // Unless it's wakeup time
			break;

		case MASTER_WAKE:
			DEBUG_PRINTLN(F("WAKE"));
			info.vcc = readVcc();  //заранее
			slaveI2C.begin();		// Now we are slave for the ESP
			wakeESP();
			masterWokenUpAt = millis(); 
			state = SENDING;
			break;

		case SENDING:
			DEBUG_PRINTLN(F("SENDING"));

			if (slaveI2C.masterGoingToSleep()) 
			{
				if (slaveI2C.masterGotOurData()) 
					storage.clear();		// If Master has confirmed our data. We can start with new measurements
				secondsSleeping = 0;
				state = SLEEP;
			}
			if (millis() - masterWokenUpAt > GIVEUP_ON_MASTER_AFTER * 1000UL) 
			{
				secondsSleeping = 0;
				state = SLEEP;
			}
			break;
	}
}