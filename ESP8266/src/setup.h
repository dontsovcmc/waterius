#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>

/*
	Включить отправку данных на свой TCP сервер. см. sender_tcp.h
*/
//#define SEND_TCP   

/*
	Включить отправку данных в приложение Blynk.cc
*/
#define SEND_BLYNK


/*
	Уровень логирования
*/
#define LOGLEVEL 6

/*
	Время ответа сервера
*/
#define SERVER_TIMEOUT 7000UL // ms

/*
	Время подключения к точке доступа
*/
#define ESP_CONNECT_TIMEOUT 10000UL

#define I2C_SLAVE_ADDR 10

#define VER_1 1
#define VER_2 2
#define CURRENT_VERSION VER_2


#define KEY_LEN 34
#define HOSTNAME_LEN 32

#define EMAIL_LEN 32
#define EMAIL_TITLE_LEN 64
#define EMAIL_TEMPLATE_LEN 200

struct Settings
{
	uint8_t  version;
	uint8_t  reserved;  //x16 bit
	
	/*uint32_t ip;
	uint32_t subnet;
	uint32_t gw;*/
	
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

	/*
	SEND_BLYNK: Если email не пустой, то отсылается e-mail
	SEND_TCP: не используется	
	*/
	char     email[EMAIL_LEN];
	
	/*
	SEND_BLYNK: Заголовок письма. {V0}-{V4} заменяются на данные 
	SEND_TCP: не используется	
	*/
	char     email_title[EMAIL_TITLE_LEN];

	/*
	SEND_BLYNK: Шаблон эл. письма. {V0}-{V4} заменяются на данные 
	SEND_TCP: не используется	
	*/
	char     email_template[EMAIL_TEMPLATE_LEN];

	float    value0_start;
	float    value1_start;
	uint16_t liters_per_impuls;

	//Стартовые значения введенные пользователем
	uint32_t impules0_start;
	uint32_t impules1_start;

	//Не понятно, как получить от Blynk прирост показаний, 
	//поэтому сохраним их в памяти каждое включение
	float    prev_value0;
	float    prev_value1;

	uint16_t crc;
};

#endif