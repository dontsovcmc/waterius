
#include <Arduino.h>

#include <avr/pgmspace.h>
#include <stdlib.h>
#include <TinyDebugSerial.h>

#include "Setup.h"
#include <USIWire.h>
#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"

TinyDebugSerial mySerial;


static enum State state = SLEEP;
static uint16_t secondsSleeping = 0;
static unsigned long masterWokenUpAt;

static uint16_t counter = 0;

struct SensorData {
	uint16_t counter;		  
	uint16_t voltage;	      
	uint16_t temperature;     
};

Storage storage( sizeof( SensorData ) );
SlaveI2C slaveI2C;

void setup() 
{
	DEBUG_CONNECT(9600);
  	DEBUG_PRINTLN(F("==== START ===="));

	resetWatchdog(); // Needed for deep sleep to succeed
}

void loop() 
{
  	DEBUG_PRINT(F("LOOP (")); DEBUG_PRINT(state); DEBUG_PRINTLN(F(")"));

	switch ( state ) {
		case SLEEP:
			slaveI2C.end();			// We don't want to be i2c slave anymore. 
			gotoDeepSleep( MEASUREMENT_EVERY, &counter);		// Deep sleep for X seconds
			secondsSleeping += MEASUREMENT_EVERY;	// Keep a track of how many seconds we have been sleeping
			state = MEASURING;

			DEBUG_PRINT(F("COUNTER=")); DEBUG_PRINTLN(counter);
			break;
		case MEASURING:
			SensorData sensorData;
			sensorData.counter = counter;
			sensorData.voltage = readVcc();
			sensorData.temperature = 0;
			DEBUG_PRINT(F("volt=")); DEBUG_PRINTLN(sensorData.voltage);
			storage.addElement( &sensorData );

			state = SLEEP;			// Sucessfully stored the data, starting sleeping again
			if ( secondsSleeping >= WAKE_MASTER_EVERY ) 
				state = MASTER_WAKE; // Unless it's wakeup time
			break;

		case MASTER_WAKE:
			slaveI2C.begin();		// Now we are slave for the ESP
			//wakeESP();
			masterWokenUpAt = millis(); 
			state = SENDING;
			break;

		case SENDING:
			DEBUG_PRINTLN(F("SENDING"));
			delay(GIVEUP_ON_MASTER_AFTER * 1000UL);
			secondsSleeping = 0;
			state = SLEEP;
			if ( slaveI2C.masterGoingToSleep() ) {
				if ( slaveI2C.masterGotOurData() ) storage.clear();		// If Master has confirmed our data. We can start with new measurements
				secondsSleeping = 0;
				state = SLEEP;
			}
			if ( millis() - masterWokenUpAt > GIVEUP_ON_MASTER_AFTER * 1000UL ) {
				secondsSleeping = 0;
				state = SLEEP;
			}
			break;
	}
}