#include "MyWifi.h"
#include "MasterI2C.h"
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>


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

void wait_high(const uint8_t pin, const unsigned long timeout)
{
	pinMode(pin, INPUT);
	unsigned long t = millis();
	while (digitalRead(pin) == LOW && millis() - t < timeout)
	{
		delay(10);
	}
}


void setup()
{
#ifdef LOGLEVEL
	Serial.begin( 115200 ,SERIAL_8N1, SERIAL_TX_ONLY);
#endif

	ESP.wdtDisable();

	LOG_NOTICE( "ESP", "Booted" );
	//ждем, пока пользователь отпустит кнопку
	wait_high(SCL_PIN, 10000UL);
	
	LOG_NOTICE( "ESP", "Ready" );
	masterI2C.begin();
	if (masterI2C.mode == SETUP_MODE)
		LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
	else
		LOG_NOTICE( "ESP", "I2C-begined: mode TRANSMIT" );

	BLINK(200);
}


void loop()
{
	if (!myWifi.begin(masterI2C.mode))
	{
		LOG_ERROR( "ESP", "Wifi connected false, go sleep" );
		masterI2C.sendCmd( 'Z' );	// Tell slave we are going to sleep
		ESP.deepSleep( 0, RF_DEFAULT );			// Sleep until I2C slave wakes us up again.
		return;
	}
	LOG_NOTICE( "ESP", "Wifi-begined" );


	// Get slave statistic like wakeuptime, measurement frequency and store them in the beginning of the transmissionbuffer
	Header header = masterI2C.getHeader(); 
	header.deviceID = myWifi.deviceID;
	header.devicePWD = myWifi.devicePWD;

	memcpy( buffer, &header, sizeof( header ) );

	LOG_NOTICE( "Stat: bytesReady", header.bytesReady);
	LOG_NOTICE( "Stat: voltage", header.vcc);

	// Read all stored measurements from slave and place after statistics in buffer after statistics
	uint16_t bytesRead = masterI2C.getSlaveStorage( buffer + sizeof(header), sizeof(buffer), header.bytesReady );
	Header *ss = (Header*)(&buffer);
	if ( bytesRead > 0 ) 
	{
		ss->message_type = ATTINY_OK;
		if (myWifi.send( buffer, bytesRead + sizeof(header)))  // Try to send them to the server.
			masterI2C.sendCmd( 'A' ); // Tell slave that we succesfully passed the data on to the server. He can delete it.
	}
	else
	{
		ss->message_type = ATTINY_FAIL;
		myWifi.send( buffer, sizeof(header)); // Try to send error
		BLINK(50); delay(100); BLINK(50);
	}
	masterI2C.sendCmd( 'Z' );	// Tell slave we are going to sleep

	//WiFi.disconnect( true );
	LOG_NOTICE( "ESP", "Going to sleep" );
	ESP.deepSleep( 0, RF_DEFAULT );			// Sleep until I2C slave wakes us up again.
}
