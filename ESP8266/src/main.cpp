
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>

#include "wifi_settings.h"
#include "master_i2c.h"
#include "setup_ap.h"
#include "sender_blynk.h"
#include "sender_tcp.h"

MasterI2C masterI2C; // Для общения с Attiny85 по i2c

SlaveData data; // Данные от Attiny85
Settings sett;  // Настройки соединения и предыдущие показания из EEPROM

void setup()
{
	memset(&data, 0, sizeof(data)); // На всякий случай
	LOG_BEGIN(115200);
	LOG_NOTICE("ESP", "Booted");
	ESP.wdtDisable();
	masterI2C.begin();
}

void calculate_values(Settings &sett, SlaveData &data, float *channel0, float *channel1)
{

	LOG_NOTICE("ESP", "new impulses=" << data.impulses0 << " " << data.impulses1);

	if (sett.liters_per_impuls > 0)
	{
		*channel0 = sett.channel0_start + (data.impulses0 - sett.impules0_start) / 1000.0 * sett.liters_per_impuls;
		*channel1 = sett.channel1_start + (data.impulses1 - sett.impules1_start) / 1000.0 * sett.liters_per_impuls;
		LOG_NOTICE("ESP", "new values=" << *channel0 << " " << *channel1);
	}
}

void loop()
{
	float channel0, channel1;

	data.diagnostic = masterI2C.getSlaveData(data); //нужны и в настройке

	// Режим настройки, запускаем точку доступа на 192.168.4.1
	if (masterI2C.setup_mode())
	{
		loadConfig(sett);

		//Вычисляем текущие показания
		calculate_values(sett, data, &channel0, &channel1);
		setup_ap(sett, data, channel0, channel1);
	}
	else
	{   // Режим передачи новых показаний
		if (!loadConfig(sett)) {
			LOG_ERROR("ESP", "error loading config");
		}
		else {
			//Вычисляем текущие показания
			calculate_values(sett, data, &channel0, &channel1);

			LOG_NOTICE("WIF", "Starting Wifi");
			//WiFi.mode(WIFI_STA);
			
			//WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
			WiFi.begin(); 

			//Ожидаем подключения к точке доступа
			uint32_t start = millis();
			while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT)
			{
				LOG_NOTICE("WIF", "Wifi status: " << WiFi.status());
				delay(200);
			}

			if (WiFi.status() == WL_CONNECTED)
			{
				LOG_NOTICE("WIF", "Connected, got IP address: " << WiFi.localIP().toString());

#ifdef SEND_BLYNK
				if (send_blynk(sett, channel0, channel1, data.voltage))
				{
					LOG_NOTICE("BLK", "send ok");

					//Сохраним текущие значения в памяти. Для расхода за сутки.
					sett.prev_channel0 = channel0;
					sett.prev_channel1 = channel1;
					storeConfig(sett);
				}
#endif
#ifdef SEND_TCP
				if (send_tcp(sett, channel0, channel1, data.voltage / 1000.0))
				{
					LOG_NOTICE("TCP", "send ok");
				}
#endif
			}
			else {
				LOG_NOTICE("BLK", "Wi-Fi not connected");
			}
		}
	}

	LOG_NOTICE("ESP", "Going to sleep");
	masterI2C.sendCmd('Z');		  // "Можешь идти спать"
	ESP.deepSleep(0, RF_DEFAULT); // "Спим до следущего включения EN"
}
