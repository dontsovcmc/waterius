
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

	LOG_INFO(F("NTP"), F("Waiting for NTP time sync: "));
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
		LOG_INFO(F("NTP"), "Current time: " + String( asctime(&timeinfo)));
		return true;
	}
	return false;
}


void print_wifi_mode()
{
    //WiFi.setPhyMode(WIFI_PHY_MODE_11B = 1, WIFI_PHY_MODE_11G = 2, WIFI_PHY_MODE_11N = 3);
    WiFiPhyMode_t m = WiFi.getPhyMode();
	switch (m) {
		case WIFI_PHY_MODE_11B: 
			LOG_INFO(FPSTR(S_WIF), F("mode B"));
			break;
		case WIFI_PHY_MODE_11G: 
			LOG_INFO(FPSTR(S_WIF), F("mode G"));
			break;
		case WIFI_PHY_MODE_11N: 
			LOG_INFO(FPSTR(S_WIF), F("mode N"));
			break;
		default:
			LOG_INFO(FPSTR(S_WIF), "mode " + String( (int)m));
		break;
	}
}