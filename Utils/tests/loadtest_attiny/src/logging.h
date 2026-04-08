#ifndef _WATERIUS_LOGGING_h
#define _WATERIUS_LOGGING_h

#include <Arduino.h>

// Streaming operator for serial print use. 
template<class T> inline Print &operator <<( Print &obj, T arg ) {
	obj.print( arg ); return obj;
}
#define endl "\r\n"

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

// Default do no logging...
#define LOG_BEGIN(baud) do {} while (0)
#define LOG_END() do {} while (0)
#define LOG_ERROR(content)     do {} while (0)
#define LOG_INFO(content)      do {} while (0)
#define LOG_DEBUG(content)	    do {} while (0)

// Depending on log level, add code for logging
#ifdef LOGLEVEL
	#undef LOG_BEGIN
	#define LOG_BEGIN(baud) do { Serial.begin(baud, SERIAL_8N1); } while(0)
	#undef LOG_END
	#define LOG_END() do { Serial.flush(); Serial.end(); } while(0)
	#undef LOG_ERROR
	#define LOG_ERROR(content) do { LOG_FORMAT_TIME; Serial << "  ERROR: " << content << endl; } while(0)
	#if LOGLEVEL >= 1
		#undef LOG_INFO
		#define LOG_INFO(content) do { LOG_FORMAT_TIME; Serial << "  INFO: " << content << endl; } while(0)
		#if LOGLEVEL >= 2
			#undef LOG_DEBUG
			#define LOG_DEBUG(content) do { LOG_FORMAT_TIME; Serial << "  DEBUG: " << content << endl; } while(0)
		#endif // LOGLEVEL >= 2
	#endif // LOGLEVEL >= 1
#endif // LOGLEVEL >= 0

#endif

