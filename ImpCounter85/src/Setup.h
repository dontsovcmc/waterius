#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>
#include "TinyDebugSerial.h"

#define STORAGE_SIZE 30  //байт. Размер кольцевого буфера для измерений (измерение=4 байта)

struct Data {
	uint32_t value0;
	uint32_t value1;
};

struct Header {
	uint8_t  version;
	uint8_t  service;
	uint32_t voltage;
	Data     data;
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