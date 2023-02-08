#include <ESP8266WiFi.h>
#include "utils.h"
#include "Logging.h"
#include "time.h"
#include "porting.h"

void print_wifi_mode()
{
	// WiFi.setPhyMode(WIFI_PHY_MODE_11B = 1, WIFI_PHY_MODE_11G = 2, WIFI_PHY_MODE_11N = 3);
	WiFiPhyMode_t m = WiFi.getPhyMode();
	switch (m)
	{
	case WIFI_PHY_MODE_11B:
		LOG_INFO(F("mode B"));
		break;
	case WIFI_PHY_MODE_11G:
		LOG_INFO(F("mode G"));
		break;
	case WIFI_PHY_MODE_11N:
		LOG_INFO(F("mode N"));
		break;
	default:
		LOG_INFO(F("mode ") << (int)m);
		break;
	}
}

void set_hostname()
{
	String host_name = get_device_name();
	if (!WiFi.hostname(host_name.c_str()))
		LOG_ERROR(F("Set hostname failed"));
	LOG_INFO(F("hostname ") + String(WiFi.hostname()));
}

/**
 * @brief Форимрует строку с именем устройства
 * в виде ИМЯ_БРЕНДА%-ИДЕНТИФИКАТОР_ЧИПА,
 * пример waterius-12346
 *
 * @return строку с уникальным именем устройства
 */
String get_device_name()
{
	String deviceName = String(BRAND_NAME) + "-" + getChipId();
	return deviceName;
}


/**
 * @brief Преобразует MAC адрес в шестнадцатиричный вид без разделителей. Например AABBCCDDEEFF
 *
 * @return строка с MAC адресом
 */
String get_mac_address_hex()
{
	uint8_t baseMac[6];
	char baseMacChr[13] = {0};
	WiFi.macAddress(baseMac);
	sprintf(baseMacChr, MAC_STR_HEX, baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
	return String(baseMacChr);
}

/**
 * @brief Рассчитывает чексуму объекта настроек
 *  за пример взят следующий код https://github.com/renatoaloi/EtherEncLib/blob/master/checksum.c
 * @param  sett настройки программы
 * @return возвращет чексуму объекта настроек
 */
uint16_t get_checksum(const Settings &sett)
{
	uint8_t *buf = (uint8_t *)&sett;
	uint16_t crc = 0xffff, poly = 0xa001;
	uint16_t i = 0; 
	uint16_t len = sizeof(sett) - 2;

	for(i=0; i<len; i++)
	{
		crc ^= buf[i];
		for(uint8_t j=0; j<8; j++)
		{
			if(crc & 0x01)
			{
				crc >>= 1;
				crc ^= poly;
			}
			else
				crc >>= 1;
		}
	}
	LOG_INFO(F("get_checksum crc=") << crc);
	return crc; 
}

/**
 * @brief Возвращает протокол ссылки в нижнем регистре
 *
 * @param url ссылка
 * @return строка содержащая название протокола в нижнем регистре http или https
 */
String get_proto(const String &url)
{
	String proto = "";
	int index = url.indexOf(':');
	if (index > 0)
	{
		proto = url.substring(0, index);
		proto.toLowerCase();
	}
	return proto;
}

/**
 * @brief убирает в коце строки слэш
 * 
 * @param topic стркоа с MQTT топиком
 */
void remove_trailing_slash(String &topic){
    if (topic.endsWith(F("/")))
    {
        topic.remove(topic.length() - 1);
    }
   
}

/**
 * @brief Возвращает признак является ли адрес адресом сайта Ватериуса
 * 
 * @param url адрес сайта
 * @return true сайт является сайтом Ватериуса
 * @return false сайт НЕ является сайтом Ватериуса
 */
bool is_waterius_site(const String &url)
{
	String temp_str = url;
	temp_str.toLowerCase();
	// специально не используется WATERIUS_DEFAULT_DOMAIN
	return temp_str.startsWith(F("https://cloud.waterius.ru"));
}

/**
 * @brief Возвращает признак настроена ли интеграция с Blynk
 * 
 * @param sett настройки устройства
 * @return true настроена интеграция с Blynk
 * @return false НЕ настроена интеграция с Blynk
 */
bool is_blynk(const Settings &sett)
{
#ifndef BLYNK_DISABLED
	return sett.blynk_host[0] && sett.blynk_key[0];
#else
	return false;
#endif
}

/**
 * @brief Возвращает признак настроена ли интеграция с MQTT
 * 
 * @param sett настройки устройства
 * @return true настроена интеграция с MQTT
 * @return false настроена интеграция с MQTT
 */
bool is_mqtt(const Settings &sett)
{
#ifndef MQTT_DISABLED
	return sett.mqtt_host[0];
#else
	return false;
#endif
}

/**
 * @brief Возвращает признак настроена ли интеграция с HomeAssistant
 * 
 * @param sett настройки устройства
 * @return true настроена интеграция с HomeAssistant
 * @return false настроена интеграция с HomeAssistant
 */
bool is_ha(const Settings &sett)
{
#ifndef MQTT_DISABLED
	return is_mqtt(sett) && sett.mqtt_auto_discovery;
#else
	return false;
#endif
}
