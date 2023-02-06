#ifndef _WATERIUS_LOGGING_h
#define _WATERIUS_LOGGING_h

#include "setup.h"

#include <Arduino.h>

template <class T>
inline Print &operator<<(Print &obj, T arg)
{
	obj.print(arg);
	return obj;
}

#define MS_IN_DAY 86400000
#define MS_IN_HOUR 3600000
#define MS_IN_MINUTE 60000
#define MS_IN_SECOND 1000
#define LOG_FORMAT_TIME                                                                               \
	do                                                                                                \
	{                                                                                                 \
		unsigned long logTime = millis();                                                             \
		unsigned char minutes = ((logTime % MS_IN_DAY) % MS_IN_HOUR) / MS_IN_MINUTE;                  \
		unsigned char seconds = (((logTime % MS_IN_DAY) % MS_IN_HOUR) % MS_IN_MINUTE) / MS_IN_SECOND; \
		unsigned short ms = ((((logTime % MS_IN_DAY) % MS_IN_HOUR) % MS_IN_MINUTE) % MS_IN_SECOND);   \
		char logFormattedTime[17];                                                                    \
		sprintf(logFormattedTime, "%02u:%02u:%03u", minutes, seconds, ms);                            \
		Serial << String(logFormattedTime);                                                           \
	} while (0)

// Default do no logging...
#define LOG_BEGIN(baud)                 \
	do                                  \
	{                                   \
		Serial.begin(baud, SERIAL_8N1); \
	} while (0)
#define LOG_END()       \
	do                  \
	{                   \
		Serial.flush(); \
		Serial.end();   \
	} while (0)
#define LOG_INFO(content) \
	do                    \
	{                     \
	} while (0)

#define LOG_ERROR(content) \
	do                     \
	{                      \
	} while (0)

#ifdef LOGLEVEL
// Depending on log level, add code for logging
#undef LOG_ERROR
#define LOG_ERROR(content)                           \
	do                                               \
	{                                                \
		LOG_FORMAT_TIME;                             \
		Serial << "  ERROR : " << content << "\r\n"; \
	} while (0)
#undef LOG_INFO
#define LOG_INFO(content)                            \
	do                                               \
	{                                                \
		LOG_FORMAT_TIME;                             \
		Serial << "  INFO  : " << content << "\r\n"; \
	} while (0)

#endif // LOGLEVEL >= 0
#endif