#ifndef _WATERIUS_LOGGING_h
#define _WATERIUS_LOGGING_h

#include "setup.h"
#define LOG_TIME_FORMAT 2

#include <Arduino.h>

#include <uuid/common.h>
#include <uuid/log.h>

#include <time.h>
#include <atomic>
#include <list>
#include <memory>
#include <string>

// Streaming operator for serial print use. 
template<class T> inline Print &operator <<( Print &obj, T arg ) {
	obj.print( arg ); return obj;
}
//#define endl "\r\n"

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
const char S_TKN[]                  PROGMEM = "TKN";


static uuid::log::Logger log_esp{FPSTR(S_ESP)};
static uuid::log::Logger log_cfg{FPSTR(S_CFG)};
static uuid::log::Logger log_ap{FPSTR(S_AP)};
static uuid::log::Logger log_wif{FPSTR(S_WIF)};
static uuid::log::Logger log_blk{FPSTR(S_BLK)};
static uuid::log::Logger log_mqt{FPSTR(S_MQT)};
static uuid::log::Logger log_i2c{FPSTR(S_I2C)};
static uuid::log::Logger log_snd{FPSTR(S_SND)};
static uuid::log::Logger log_rqt{FPSTR(S_RQT)};
static uuid::log::Logger log_ntp{FPSTR(S_NTP)};
static uuid::log::Logger log_tkn{FPSTR(S_TKN)};


class SerialLogHandler: public uuid::log::Handler {
public:
	static constexpr size_t MAX_LOG_MESSAGES = 50; /*!< Maximum number of log messages to buffer before they are output. @since 1.0.0 */
	static constexpr uint16_t DEFAULT_PORT = 514; /*!< Default UDP port to send messages to. @since 1.0.0 */
	static constexpr uint16_t MAX_BUFFER_SIZE = 4096;
	char* ring_buffer;
	int index_r=0;
	int index_w=0;
	bool overflow_buffer=false;
	uint8_t cacheLevel=-1;

	SerialLogHandler() = default;

	void start();

	void operator<<(std::shared_ptr<uuid::log::Message> message);
};

static SerialLogHandler log_handler;

// Depending on log level, add code for logging
#define LOG_BEGIN(baud) do { Serial.begin(baud, SERIAL_8N1); } while(0)
#define LOG_END() do { Serial.flush(); Serial.end(); } while(0)

#endif