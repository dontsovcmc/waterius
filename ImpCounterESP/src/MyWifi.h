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
	 uint16_t  deviceID;
	 uint16_t  devicePWD;

	 void storeConfig();
	 bool loadConfig();

 public:
	 bool begin();
	 bool send( const void * data, uint16_t length );
};


#endif

