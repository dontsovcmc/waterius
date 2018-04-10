#include "MyWifi.h"
#include "MasterI2C.h"
#include "Logging.h"
#include <user_interface.h>


#ifdef LOGLEVEL
void BLINK(uint16_t msec) {}
#else
void BLINK(uint16_t msec)
{
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, HIGH);
	delay(msec);
	digitalWrite(LED_PIN, LOW);
	pinMode(LED_PIN, INPUT);
}
#endif

MasterI2C masterI2C;
MyWifi myWifi;
byte buffer[512];


void setup()
{
	ESP.wdtDisable();
	pinMode(SETUP_PIN, OUTPUT);
	digitalWrite(SETUP_PIN, LOW);
	pinMode(SETUP_PIN, INPUT);
	
	LOG_NOTICE( "ESP", "Booted" );
	BLINK(200);
	Serial.begin( 115200 ,SERIAL_8N1, SERIAL_TX_ONLY); Serial.println();

}


void loop()
{
	if (!myWifi.begin())
	{
		LOG_ERROR( "ESP", "Wifi connected false, go sleep" );
		BLINK(200);
		delay(100);
		BLINK(200);
		ESP.deepSleep( 0, RF_DEFAULT );			// Sleep until I2C slave wakes us up again.
		return;
	}
	LOG_NOTICE( "ESP", "Wifi-begined" );

	masterI2C.begin();
	LOG_NOTICE( "ESP", "I2C-begined" );

	// Get slave statistic like wakeuptime, measurement frequency and store them in the beginning of the transmissionbuffer
	SlaveStats slaveStats = masterI2C.getSlaveStats(); 
	slaveStats.deviceID = myWifi.deviceID;
	slaveStats.devicePWD = myWifi.devicePWD;

	memcpy( buffer, &slaveStats, sizeof( slaveStats ) );

	LOG_NOTICE( "Stat: bytesReady", slaveStats.bytesReady);
	LOG_NOTICE( "Stat: voltage", slaveStats.vcc);

	// Read all stored measurements from slave and place after statistics in buffer after statistics
	uint16_t bytesRead = masterI2C.getSlaveStorage( buffer + sizeof(slaveStats), sizeof(buffer), slaveStats.bytesReady );
	if ( bytesRead > 0 ) 
	{
		if (myWifi.send( buffer, bytesRead + sizeof(slaveStats)))  // Try to send them to the server.
		{
			masterI2C.sendCmd( 'A' ); // Tell slave that we succesfully passed the data on to the server. He can delete it.
			BLINK(50);
		}
        else
		{
			BLINK(50);delay(100);
			BLINK(50);
		}    
	}
	else
	{
		BLINK(50);delay(100);
		BLINK(50);delay(100);
		BLINK(50);
	}
	masterI2C.sendCmd( 'Z' );	// Tell slave we are going to sleep

	LOG_NOTICE( "ESP", "Going to sleep" );
	ESP.deepSleep( 0, RF_DEFAULT );			// Sleep until I2C slave wakes us up again.
}
