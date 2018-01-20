#include "Setup.h"
#include <USIWire.h>
#include "Sensor.h"
#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"


// Format for a measurement
struct SensorData {
	uint16_t counter;		  // We don't need float precision. Saving storage
	uint16_t voltage;	      // raw data
	uint16_t temperature;     // not implemented
};


// Global objects
Storage storage( sizeof( SensorData ) );
SlaveI2C slaveI2C;

void setup() {
	pinMode( ESP_RESET_PIN, INPUT_PULLUP );
	resetWatchdog(); // Needed for deep sleep to succeed
}

void loop() {

	static enum State { // Our state machine
		SLEEP,
		MEASURING,
		MASTER_WAKE,
		SENDING
	} state = SLEEP;

	static uint16_t secondsSleeping = 0;
	static unsigned long masterWokenUpAt;
	static uint16_t counter = 0;

	switch ( state ) {
		case SLEEP:
			//sensorPower( false );	// Save power by turning off power to sensors
			slaveI2C.end();			// We don't want to be i2c slave anymore. 
			gotoDeepSleep( MEASUREMENT_EVERY, &counter);		// Deep sleep for X seconds
			secondsSleeping += MEASUREMENT_EVERY;	// Keep a track of how many seconds we have been sleeping
			state = MEASURING;
			break;
		case MEASURING:
			SensorData sensorData;
			
			//sensorPower( true );	// Power up i2c devices and read data from them
			sensorData.counter = counter;
			sensorData.voltage = readVcc();
			sensorData.temperature = 0;
			//sensorPower( false );	// Turn off i2c device again
			storage.addElement( &sensorData );

			state = SLEEP;			// Sucessfully stored the data, starting sleeping again
			if ( secondsSleeping >= WAKE_MASTER_EVERY ) state = MASTER_WAKE; // Unless it's wakeup time
			break;
		case MASTER_WAKE:
			sensorPower( true );	// Sensors needs to have power otherwise they drag down the I2C
			slaveI2C.begin();		// Now we are slave for the ESP
			wakeESP();
			masterWokenUpAt = millis(); 
			state = SENDING;
			break;
		case SENDING:
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