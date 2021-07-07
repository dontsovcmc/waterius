#ifndef _WATERIUS_LOGGING_h
#define _WATERIUS_LOGGING_h

#include "setup.h"
#define LOG_TIME_FORMAT 2

#define LOG(text) do { Serial.print(text); } while (0)

const char S_ESP[]                  PROGMEM = "ESP";
const char S_CFG[]                  PROGMEM = "CFG";
const char S_AP[]                   PROGMEM = "AP";
const char S_WIF[]                  PROGMEM = "WIF";
const char S_BLK[]                  PROGMEM = "BLK";
const char S_MQT[]                  PROGMEM = "MQT";
const char S_I2C[]                  PROGMEM = "I2C";
const char S_SND[]                  PROGMEM = "SND";
const char S_RQT[]                  PROGMEM = "RQT";
const char S_NTP[]                  PROGMEM = "NTP";
const char S_INFO[]                 PROGMEM = "  INFO      (";
const char S_ERROR[]                PROGMEM = "  ERROR     (";
const char S_DEBUG[]                PROGMEM = "  DEBUG     (";
const char S_SVC[]                  PROGMEM = " ) : ";

void LOG_BEGIN(unsigned long baud);
void LOG_END();
void LOG_START(String group, String svc);

#endif
