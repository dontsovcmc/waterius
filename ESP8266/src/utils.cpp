
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

unsigned int CRC16(unsigned int crc, char *buf, const int len)
{
	for (int pos = 0; pos < len; pos++)
	{
		crc ^= (unsigned int)buf[pos];    // XOR byte into least sig. byte of crc

		for (int i = 8; i != 0; i--) {    // Loop over each bit
			if ((crc & 0x0001) != 0) {      // If the LSB is set
				crc >>= 1;                    // Shift right and XOR 0xA001
				crc ^= 0xA001;
			}
			else                            // Else LSB is not set
				crc >>= 1;                    // Just shift right
		}
	}

	return crc;
}