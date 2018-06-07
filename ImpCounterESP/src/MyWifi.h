#ifndef _WIFI_h
#define _WIFI_h

#include "setup.h"

#include <Arduino.h>
#include <WiFiClient.h>

#define KEY_LEN 34
#define HOSTNAME_LEN 32

#define VER_1 1
#define CURRENT_VERSION VER_1

struct Settings
{
	uint8_t  version;
	uint8_t  reserved;  //x16 bit
	uint32_t ip;
	uint32_t subnet;
	uint32_t gw;
	uint32_t server;
	char  key[KEY_LEN];
	char  hostname[HOSTNAME_LEN];

	float    litres0_start;
	float    litres1_start;
	uint16_t liters_per_impuls;

	//store values
	uint32_t impules0_start;
	uint32_t impules1_start;
	uint32_t value0; 
	uint32_t value1;

	uint16_t crc;
};

void storeConfig(const Settings &sett);
bool loadConfig(Settings &sett);


class MyWifi
{
protected:

public:
	bool begin(const uint8_t mode);
	bool send(const Settings &sett, const void * data, uint16_t length);
	void setup(Settings &sett);
	bool connect(const Settings &sett);
};


#endif

