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
	 uint16_t remotePort;

	 void storeConfig( IPAddress ip, IPAddress subnet, IPAddress gw, IPAddress remoteIP, uint16_t remotePort);
	 bool loadConfig();

 public:
	 bool begin();
	 bool send( const void * data, uint16_t length );
};


#endif

