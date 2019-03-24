
#include "utils.h"

#include "Logging.h"
#include "string.h"
#include "time.h"


bool setClock() 
{
	configTime(0, 0, "ru.pool.ntp.org", "time.nist.gov");

	LOG_NOTICE("NTP", "Waiting for NTP time sync: ");
	uint32_t start = millis();
	time_t now = time(nullptr);
	
	while (now < 8 * 3600 * 2  && millis() - start < ESP_CONNECT_TIMEOUT) {
		delay(500);
		now = time(nullptr);
	}
	
	if (millis() - start >= ESP_CONNECT_TIMEOUT) {
		return false;
	}

	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
	LOG_NOTICE("NTP", "Current time: " << asctime(&timeinfo));
	return true;
}
