
#include "utils.h"

#include "Logging.h"
#include "string.h"
#include "time.h"

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
		LOG_INFO(F("NTP"), F("Current time: ") << asctime(&timeinfo));
		return true;
	}
	return false;
}
