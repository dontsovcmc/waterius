#ifndef _WATERIUS_LOGGING_h
#define _WATERIUS_LOGGING_h

#include "setup.h"
#define LOG_TIME_FORMAT 2

#include <Arduino.h>

// Streaming operator for serial print use. 
template<class T> inline Print &operator <<( Print &obj, T arg ) {
	obj.print( arg ); return obj;
}
#define endl "\r\n"

const char S_ESP[]                  PROGMEM = "ESP";
const char S_CFG[]                  PROGMEM = "CFG";
const char S_AP[]                   PROGMEM = "AP";
const char S_WIF[]                  PROGMEM = "WIF";
const char S_BLK[]                  PROGMEM = "BLK";
const char S_MQT[]                  PROGMEM = "MQT";
const char S_I2C[]                  PROGMEM = "I2C";
const char S_SND[]                  PROGMEM = "SND";
const char S_RQT[]                  PROGMEM = "RQT";


/* Generate and print the trailing log timestamp.
		1 = (1234), showing time in seconds since boot. Generates lightweight inline code.
		2 = (001:02:03:04:005) ( day 1, hour 2, minute 3, second 4, millisecond 5 ). Much bigger code, but very readable
*/
#if LOG_TIME_FORMAT == 1
	#define LOG_FORMAT_TIME do { Serial << millis() / 1000; } while (0)
#elif LOG_TIME_FORMAT == 2 
	#define MS_IN_DAY 86400000
	#define MS_IN_HOUR 3600000
	#define MS_IN_MINUTE 60000
	#define MS_IN_SECOND  1000
	#define LOG_FORMAT_TIME do \
	{ \
		unsigned long logTime = millis(); \
		unsigned short days = logTime / MS_IN_DAY; \
		unsigned char hours = ( logTime % MS_IN_DAY ) / MS_IN_HOUR; \
		unsigned char minutes = ( ( logTime % MS_IN_DAY ) % MS_IN_HOUR ) / MS_IN_MINUTE; \
		unsigned char seconds = ( ( ( logTime % MS_IN_DAY ) % MS_IN_HOUR ) % MS_IN_MINUTE ) / MS_IN_SECOND; \
		unsigned short ms = ( ( ( ( logTime % MS_IN_DAY ) % MS_IN_HOUR ) % MS_IN_MINUTE ) % MS_IN_SECOND ); \
		char logFormattedTime[17]; \
		sprintf( logFormattedTime, "%03u:%02u:%02u:%02u:%03u", days, hours, minutes, seconds, ms ); \
		Serial << logFormattedTime; \
	} while (0)
#endif // LOG_TIME_FORMAT

// Default do no logging...
#define LOG_BEGIN(baud) do {} while (0)
#define LOG_END() do {} while (0)
#define LOG_ERROR(svc, content)     do {} while (0)
#define LOG_INFO(svc, content)      do {} while (0)
#define LOG_DEBUG(svc, content)	    do {} while (0)

// Depending on log level, add code for logging
#ifdef LOGLEVEL
	#undef LOG_BEGIN
	#define LOG_BEGIN(baud) do { Serial.begin(baud, SERIAL_8N1); } while(0)
	#undef LOG_END
	#define LOG_END() do { Serial.flush(); Serial.end(); } while(0)
	#undef LOG_ERROR
	#define LOG_ERROR(svc, content) do { LOG_FORMAT_TIME; Serial << "  ERROR     (" << svc << ") : " << content << endl; } while(0)
	#if LOGLEVEL >= 1
		#undef LOG_INFO
		#define LOG_INFO(svc, content) do { LOG_FORMAT_TIME; Serial << "  INFO      (" << svc << ") : " << content << endl; } while(0)
		#if LOGLEVEL >= 2
			#undef LOG_DEBUG
			#define LOG_DEBUG(svc, content) do { LOG_FORMAT_TIME; Serial << "  DEBUG     (" << svc << ") : " << content << endl; } while(0)
		#endif // LOGLEVEL >= 2
	#endif // LOGLEVEL >= 1
#endif // LOGLEVEL >= 0

#endif

