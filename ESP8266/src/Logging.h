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

const char S_STATE_BAD[]            PROGMEM = "\"Не подключен\"";
const char S_STATE_CONNECTED[]      PROGMEM = "\"Подключен\"";
const char S_STATE_NULL[]           PROGMEM = "\"\"";

const char HTTP_TEXT_PLAIN[]        PROGMEM = "text/plain";
const char S_STATES[]               PROGMEM = "/states";
const char S_CAPTIVE_PORTAL[]       PROGMEM = "User requested captive portal";
const char S_GENERATE_WATERIUS_KEY[] PROGMEM = "Generate waterius key";
const char S_START_CONFIG_PORTAL[]  PROGMEM = "start config portal";
const char S_CONNECTED_TO_WIFI[]    PROGMEM = "Connected to wifi. Save settings, go to sleep";
const char S_STARTING[]             PROGMEM = "Starting";
const char S_SEND_OK[]              PROGMEM = "send ok";
const char S_GOING_SLEEP[]          PROGMEM = "Going to sleep";
const char S_ERROR_LOAD_CFG[]       PROGMEM = "error loading config";
const char S_BOOTED[]               PROGMEM = "Booted";
const char S_DATA_FAILED[]          PROGMEM = "data failed";
const char S_I2C_FAILED[]           PROGMEM = "get mode failed. Check i2c line.";
const char S_I2C_REQ_FAILED[]       PROGMEM = "requestFrom failed";
const char S_I2C_WR_FAILED[]        PROGMEM = "write cmd failed";
const char S_SKIP[]                 PROGMEM = "SKIP";
const char S_RUN[]                  PROGMEM = "run";
const char S_VIRT_OK[]              PROGMEM = "virtualWrite OK";
const char S_EMAIL_SEND[]           PROGMEM = "email was send";
const char S_DISCONNECTED[]         PROGMEM = "disconnected";
const char S_CONNECT_ERROR[]        PROGMEM = "connect error";
const char S_CRC_OK[]               PROGMEM = "CRC ok";

const char S_WATERIUS[]  PROGMEM = "WATERIUS.RU";
const char S_BLYNK[]  PROGMEM = "BLYNK.CC";
const char S_MQTT[]  PROGMEM = "MQTT";
const char S_COUNTERS[]  PROGMEM = "COUNTERS";

const char S_V0[] PROGMEM = "{V0}";
const char S_V1[] PROGMEM = "{V1}";
const char S_V2[] PROGMEM = "{V2}";
const char S_V3[] PROGMEM = "{V3}";
const char S_V4[] PROGMEM = "{V4}";
const char S_V5[] PROGMEM = "{V5}";
const char S_V6[] PROGMEM = "{V6}";
const char S_V7[] PROGMEM = "{V7}";
const char S_V8[] PROGMEM = "{V8}";



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
#define LOG_EMERGENCY(svc, content) do {} while (0)
#define LOG_ALERT(svc, content)     do {} while (0)
#define LOG_CRITICAL(svc, content)  do {} while (0)
#define LOG_ERROR(svc, content)     do {} while (0)
#define LOG_WARNING(svc, content)   do {} while (0)
#define LOG_NOTICE(svc, content)    do {} while (0)
#define LOG_INFO(svc, content)      do {} while (0)
#define LOG_DEBUG(svc, content)	    do {} while (0)

// Depending on log level, add code for logging
#if LOGLEVEL >= 0
	#undef LOG_BEGIN
	#define LOG_BEGIN(baud) do { Serial.begin( baud ,SERIAL_8N1, SERIAL_TX_ONLY); } while(0)
	#undef LOG_END
	#define LOG_END() do { Serial.flush(); Serial.end(); } while(0)
	#undef LOG_EMERGENCY
	#define LOG_EMERGENCY(svc, content)	do { LOG_FORMAT_TIME; Serial << "  EMERGENCY (" << svc << ") : " << content << endl; } while(0)
	#if LOGLEVEL >=1 
		#undef LOG_ALERT
		#define LOG_ALERT(svc, content)	do { LOG_FORMAT_TIME; Serial << "  ALERT     (" << svc << ") : " << content << endl; } while(0)
		#if LOGLEVEL >= 2
			#undef LOG_CRITICAL
			#define LOG_CRITICAL(svc, content) do { LOG_FORMAT_TIME; Serial << "  CRITICAL  (" << svc << ") : " << content << endl; } while(0)
			#if LOGLEVEL >= 3
				#undef LOG_ERROR
				#define LOG_ERROR(svc, content) do { LOG_FORMAT_TIME; Serial << "  ERROR     (" << svc << ") : " << content << endl; } while(0)
				#if LOGLEVEL >= 4
					#undef LOG_WARNING
					#define LOG_WARNING(svc, content) do { LOG_FORMAT_TIME; Serial << "  WARNING   (" << svc << ") : " << content << endl; } while(0)
					#if LOGLEVEL >= 5
						#undef LOG_NOTICE
						#define LOG_NOTICE(svc, content) do { LOG_FORMAT_TIME; Serial << "  NOTICE    (" << svc << ") : " << content << endl; } while(0)
						#if LOGLEVEL >= 6
							#undef LOG_INFO
							#define LOG_INFO(svc, content) do { LOG_FORMAT_TIME; Serial << "  INFO      (" << svc << ") : " << content << endl; } while(0)
							#if LOGLEVEL >= 7
								#define LOG_DEBUG(svc, content) do { LOG_FORMAT_TIME; Serial << "  DEBUG     (" << svc << ") : " << content << endl; } while(0)
							#endif // LOGLEVEL >= 7
						#endif // LOGLEVEL >= 6
					#endif // LOGLEVEL >= 5
				#endif // LOGLEVEL >= 4
			#endif // LOGLEVEL >= 3
		#endif // LOGLEVEL >= 2
	#endif // LOGLEVEL >= 1
#endif // LOGLEVEL >= 0

#endif

