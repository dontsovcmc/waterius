#ifndef _WIFI_h
#define _WIFI_h

#include "setup.h"

#include <Arduino.h>
#include <WiFiClient.h>


class MyWifi
{
 protected:
	 IPAddress ip;
	 IPAddress subnet; 
	 IPAddress gw;
	 IPAddress remoteIP; 
	 uint16_t  remotePort;

	 void storeConfig();
	 bool loadConfig();

 public:
	 uint16_t  deviceID;
	 uint16_t  devicePWD;
	 bool begin();
	 bool send( const void * data, uint16_t length );
};


#endif

