#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>

#define LOGLEVEL 6

#define SERVER_TIMEOUT 3000UL // ms
#define ESP_CONNECT_TIMEOUT 10000UL
#define I2C_SLAVE_ADDR 10

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
	char     key[KEY_LEN];
	char     hostname[HOSTNAME_LEN];

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

#endif