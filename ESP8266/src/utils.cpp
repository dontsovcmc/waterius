
#include "utils.h"

#include "Logging.h"
#include "string.h"
#include "time.h"

#include <ESP8266WiFi.h>
#include "Logging.h"

#define NTP_CONNECT_TIMEOUT 3000UL

bool setClock(const char* ntp_server)
{
	configTime(0, 0, ntp_server);

	#if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_NTP)); LOG(F("Waiting for NTP time sync: ")); 
    #endif
	uint32_t start = millis();
	time_t now = time(nullptr);
	
	while (now < 8 * 3600 * 2  && millis() - start < NTP_CONNECT_TIMEOUT) {
		delay(100);
		now = time(nullptr);
	}
	
	return millis() - start < NTP_CONNECT_TIMEOUT;
}

bool setClock() 
{
	if (setClock("1.ru.pool.ntp.org") 
	    || setClock("2.ru.pool.ntp.org")
	    || setClock("pool.ntp.org")) {

		time_t now = time(nullptr);
		struct tm timeinfo;
		gmtime_r(&now, &timeinfo);
		#if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_NTP)); LOG(F("Current time: ")); LOG(asctime(&timeinfo));
    	#endif
		return true;
	}
	return false;
}


void print_wifi_mode()
{
    //WiFi.setPhyMode(WIFI_PHY_MODE_11B = 1, WIFI_PHY_MODE_11G = 2, WIFI_PHY_MODE_11N = 3);
    WiFiPhyMode_t m = WiFi.getPhyMode();
	#if LOGLEVEL>=1
	LOG_START(FPSTR(S_INFO) ,FPSTR(S_WIF));
	switch (m) {
		case WIFI_PHY_MODE_11B: 
    		LOG(F("mode B")); 
			break;
		case WIFI_PHY_MODE_11G: 
			LOG(F("mode G"));
			break;
		case WIFI_PHY_MODE_11N: 
			LOG(F("mode N"));
			break;
		default:
			LOG(F("mode ")); LOG(m);
		break;
	}
    #endif
}