#ifndef _WATERIUS_LOGGING_h
#define _WATERIUS_LOGGING_h

#include "setup.h"
#include "log_capture.h"

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
		snprintf_P(logFormattedTime, sizeof(logFormattedTime),                                        \
				   PSTR("%02u:%02u:%03u"), minutes, seconds, ms);                                     \
		log_output.print(logFormattedTime);                                                                \
	} while (0)

#ifdef LOG_FREE_HEAP
#undef LOG_FREE_HEAP
#ifdef ESP8266
#define LOG_FREE_HEAP                                                                       \
	do                                                                                      \
	{                                                                                       \
		char logHeap[18];                                                                   \
		snprintf_P(logHeap, sizeof(logHeap), PSTR("-%03d/%02d%%"), int(ESP.getFreeHeap() / 1024), ESP.getHeapFragmentation()); \
		log_output.print(logHeap);                                                              \
	} while (0)
#else
#define LOG_FREE_HEAP                                                  \
	do                                                                 \
	{                                                                  \
		char logHeap[15];                                              \
		snprintf_P(logHeap, sizeof(logHeap), PSTR("-%03d/%02d"),       \
				   ESP_getFreeHeap1024(), ESP_getHeapFragmentation()); \
		log_output.print(logHeap);                                         \
	} while (0)
#endif
#else
#undef LOG_FREE_HEAP
#define LOG_FREE_HEAP ;
#endif

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

#define LOG_DEBUG(content) \
	do                     \
	{                      \
	} while (0)

#define LOG_REQUEST(content) \
	do                       \
	{                        \
	} while (0)

#if defined(LOG_LEVEL_INFO) || defined(LOG_LEVEL_DEBUG)
// Depending on log level, add code for logging
#undef LOG_ERROR
#define LOG_ERROR(content)        \
	do                            \
	{                             \
		log_head(LogTag::error);  \
		log_output << content;    \
		log_tail();               \
	} while (0)

#undef LOG_INFO
#define LOG_INFO(content)         \
	do                            \
	{                             \
		log_head(LogTag::info);   \
		log_output << content;    \
		log_tail();               \
	} while (0)

// Строка на весь запрос портала: LOG_* обработчика дописываются в неё до его выхода
#undef LOG_REQUEST
#define LOG_REQUEST(content) \
	log_head(LogTag::info);  \
	log_output << content;   \
	LogLine log_request_line

#ifdef LOG_LEVEL_DEBUG
#undef LOG_DEBUG
#define LOG_DEBUG(content)        \
	do                            \
	{                             \
		log_head(LogTag::debug);  \
		log_output << content;    \
		log_tail();               \
	} while (0)
#endif
#endif

#endif
