#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>
#include "TinyDebugSerial.h"

// Включение логгирования с TinySerial: 3 pin TX -> RX (TTL-USB 3.3 или 5в), 9600 8N1
// При логгировании не работает счетчик2 на 3-м пине.

// #define LOG_LEVEL_ERROR
// #define LOG_LEVEL_INFO
// #define LOG_LEVEL_DEBUG

#define STORAGE_SIZE 30  //байт. Размер кольцевого буфера для измерений (измерение=4 байта)

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
	#define LOG_DEBUG(x) { PRINT_NOW(F(" [D]: ")); mySerial.println(x); }
	#define LOG_LEVEL_INFO
#endif
#ifdef LOG_LEVEL_INFO
	#define LOG_INFO(x) { PRINT_NOW(F(" [I]: ")); mySerial.println(x); }
	#define LOG_LEVEL_ERROR
#endif
#ifdef LOG_LEVEL_ERROR
	#define LOG_ERROR(x) { PRINT_NOW(F(" [E]: ")); mySerial.println(x); }
#endif

#endif