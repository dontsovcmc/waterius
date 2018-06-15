#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>

//#define SEND_TCP   
#define SEND_BLYNK

#define LOGLEVEL 6

#define SERVER_TIMEOUT 7000UL // ms

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
	
	/*
	SEND_BLYNK: уникальный ключ устройства blynk
	SEND_TCP: не используется
	*/
	char     key[KEY_LEN];

	/*
	SEND_BLYNK: сервер blynk.com или свой blynk сервер
	SEND_TCP: ip адрес или имя хоста куда слать данные
	*/
	char     hostname[HOSTNAME_LEN];

	float    value0_start;
	float    value1_start;
	uint16_t liters_per_impuls;

	//Стартовые значения введенные пользователем
	uint32_t impules0_start;
	uint32_t impules1_start;

	uint16_t crc;
};

#endif