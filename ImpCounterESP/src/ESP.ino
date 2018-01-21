#include "MyWifi.h"
#include "MasterI2C.h"
#include "Logging.h"
#include <user_interface.h>

MasterI2C masterI2C;
MyWifi myWifi;
byte buffer[512];


void setup()
{
	Serial.begin( 115200 ,SERIAL_8N1, SERIAL_TX_ONLY); Serial.println();
	LOG_NOTICE( "ESP", "Booted" );

	myWifi.begin();
	LOG_NOTICE( "ESP", "Wifi-begined" );
	masterI2C.begin();
	LOG_NOTICE( "ESP", "I2C-begined" );
}


void loop()
{
	// Get slave statistic like wakeuptime, measurementfrequency and store them in the beginning of the transmissionbuffer
	SlaveStats slaveStats = masterI2C.getSlaveStats(); 
	memcpy( buffer, &slaveStats, sizeof( slaveStats ) );

	// Read all stored measurements from slave and place after statistics in buffer after statistics
	uint16_t bytesRead = masterI2C.getSlaveStorage( buffer + sizeof(slaveStats) -1 , sizeof(buffer), slaveStats.bytesReady );	

	if ( bytesRead > 0 ) {
		bool dataSent = myWifi.send( buffer, bytesRead + sizeof(slaveStats) - 1 ); // Try to send them to the server.
		if ( dataSent ) masterI2C.sendCmd( 'A' ); // Tell slave that we succesfully passed the data on to the server. He can delete it.
	}
	masterI2C.sendCmd( 'Z' );	// Tell slave we are going to sleep

	LOG_NOTICE( "ESP", "Going to sleep" );
	ESP.deepSleep( 0, RF_DEFAULT );			// Sleep until I2C slave wakes us up again.
}