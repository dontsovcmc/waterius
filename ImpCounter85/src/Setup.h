#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>
#include "TinyDebugSerial.h"

// Включение логгирования с TinySerial: 3 pin TX -> RX (TTL-USB 3.3 или 5в), 9600 8N1
// При логгировании не работает счетчик на 3-м пине.

/*
#define LOG_LEVEL_ERROR
#define LOG_LEVEL_INFO
#define LOG_LEVEL_DEBUG
*/
#define LOG_LEVEL_DEBUG

#define ESP_POWER_PIN 1			 // Номер пина, которым будим ESP8266. Если менять на 3/4, то нужно поменять пины в прерываниях.
#define SETUP_BUTTON_PIN 2       // SCL pin

#define WAIT_ESP_MSEC   4000UL   // Сколько секунд ждем передачи данных в ESP
#define SETUP_TIME_MSEC 120000UL      // Сколько пользователь настраивает ESP

// не понятно как точно тики считать для диагностики связи с сервером.
#define WAKE_MASTER_EVERY_TICKS 100UL 
#define MEASUREMENT_EVERY_MIN 1U	  // Период измерений данных. Кратно минутам строго!

#define DEVICE_ID 2                   // Модель устройства

#define STORAGE_SIZE 120  //байт. Размер кольцевого буфера для измерений (измерение=4 байта)

enum State { 
	SLEEP, //глубокий сон
	SETUP,
	MEASURING, //сохраняем измерение
	MASTER_WAKE, //пробуждаем ESP8266, i2c
	SENDING //ждем от ESP8266 команды, i2c
};

struct Header {
	uint16_t bytesReady; 
	uint8_t  deviceID;
	uint16_t masterWakeEvery;
	uint16_t vcc;
	uint8_t  service;
	uint16_t reserved;
};


#define LOG_DEBUG(x)
#define LOG_INFO(x)  
#define LOG_ERROR(x) 
#define DEBUG_CONNECT(x)  


#if defined (LOG_LEVEL_DEBUG) || defined (LOG_LEVEL_INFO) || defined (LOG_LEVEL_ERROR)
	#define DEBUG
	class TinyDebugSerial;
  	extern TinyDebugSerial mySerial;
    #define DEBUG_CONNECT(x)  mySerial.begin(x)
    #define PRINT_NOW(x) mySerial.print(millis()); mySerial.print(x);
#endif

#ifdef LOG_LEVEL_DEBUG
	#define LOG_DEBUG(x) PRINT_NOW(F(" [D]: ")); mySerial.println(x)
	#define LOG_LEVEL_INFO
#endif
#ifdef LOG_LEVEL_INFO
	#define LOG_INFO(x) PRINT_NOW(F(" [I]: ")); mySerial.println(x)
	#define LOG_LEVEL_ERROR
#endif
#ifdef LOG_LEVEL_ERROR
	#define LOG_ERROR(x) PRINT_NOW(F(" [E]: ")); mySerial.println(x)
#endif

#endif