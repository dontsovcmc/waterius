
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
static uint16_t minSleeping = 0;
static uint16_t masterWokenUpAt;

static uint16_t counter = 0;
static uint16_t counter2 = 0;

struct SensorData {
	uint16_t counter;	     
	uint16_t counter2;     
};

//https://github.com/esp8266/Arduino/issues/1825
struct SlaveStats info = {
	0, //bytesReady
	WAKE_MASTER_EVERY_MIN,
	MEASUREMENT_EVERY_MIN,
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
	minSleeping = WAKE_MASTER_EVERY_MIN;
	state = MEASURING;
}

static uint8_t shift = 0;
static uint8_t prev_state = SLEEP;
void loop() 
{
	if (prev_state != state)
	{
  		DEBUG_PRINT("LOOP ("); 
		
		DEBUG_PRINT(state); 
		  
		DEBUG_PRINTLN(")");
		prev_state = state;
	}

	switch ( state ) {
		case SLEEP:
			slaveI2C.end();			// We don't want to be i2c slave anymore. 
			//delay(3000);
			gotoDeepSleep( MEASUREMENT_EVERY_MIN, &counter, &counter2);		// Deep sleep for X minutes
			minSleeping += MEASUREMENT_EVERY_MIN;	// Keep a track of how many minutes we have been sleeping
			state = MEASURING;

			DEBUG_PRINT("COUNTER="); DEBUG_PRINTLN(counter);
			DEBUG_PRINT("COUNTER2="); DEBUG_PRINTLN(counter2);
			break;

		case MEASURING:
			SensorData sensorData;
			sensorData.counter = counter;
			sensorData.counter2 = counter2;
			storage.addElement( &sensorData );

			state = SLEEP;			// Sucessfully stored the data, starting sleeping again
			if ( minSleeping >= WAKE_MASTER_EVERY_MIN ) 
				state = MASTER_WAKE; // Unless it's wakeup time
			break;

		case MASTER_WAKE:
			info.vcc = readVcc();  //заранее
			slaveI2C.begin();		// Now we are slave for the ESP
			wakeESP();
			masterWokenUpAt = millis(); 
			state = SENDING;
			break;

		case SENDING:
			if (slaveI2C.masterGoingToSleep()) 
			{
				if (slaveI2C.masterGotOurData()) 
					storage.clear();		// If Master has confirmed our data. We can start with new measurements
				minSleeping = 0;
				state = SLEEP;
			}

			if (millis() - masterWokenUpAt > GIVEUP_ON_MASTER_AFTER * 1000UL) 
			{
				minSleeping = 0;
				state = SLEEP;
			}
			break;
	}
}
