#include <ESP8266WiFi.h>
#include "utils.h"
#include "Logging.h"
#include "time.h"
#include "porting.h"
#include "wifi_helpers.h"

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
 * @brief Форимрует строку с названием точки доступа
 * в виде ИМЯ_БРЕНДА%-ИДЕНТИФИКАТОР_ЧИПА-ВЕРСИЯ_ПРОШИВКИ,
 * пример waterius-12346-0.11.0
 *
 * @return строку с названием точки доступа
 */

String get_ap_name()
{
	return get_device_name() + "-" + String(FIRMWARE_VERSION);
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

	for (i = 0; i < len; i++)
	{
		crc ^= buf[i];
		for (uint8_t j = 0; j < 8; j++)
		{
			if (crc & 0x01)
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
 * @brief Возвращает признак является ли ссылка https
 *
 * @param url ссылка
 * @return true если ссылка https,
 * @return false если ссылка НЕ https
 *
 */
extern bool is_https(const char *url)
{
	if (url[0])
	{
		String urlStr = String(url);
		return get_proto(urlStr) == PROTO_HTTPS;
	}
	return false;
}

/**
 * @brief убирает в коце строки слэш
 *
 * @param topic стркоа с MQTT топиком
 */
void remove_trailing_slash(String &topic)
{
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

/**
 * @brief Возвращает признак будет ли использоваться DHCP
 *
 * @param sett настройки устройства
 * @return true используется DHCP
 * @return false
 */

bool is_dhcp(const Settings &sett)
{
	return sett.ip != 0;
}

/**
 * @brief Выводит информацию о системе
 *
 */
void log_system_info()
{
	// System info
	LOG_INFO(F("------------ System Info ------------"));
	LOG_INFO(F("Sketch Size: ") << ESP.getSketchSize());
	LOG_INFO(F("Free Sketch Space: ") << ESP.getFreeSketchSpace());
	LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
	LOG_INFO(F("Settings size: ") << sizeof(Settings));
	LOG_INFO(F("------------ WiFi Info ------------"));
	LOG_INFO(F("WIFI: SSID: ") << WiFi.SSID());
	LOG_INFO(F("WIFI: BSID: ") << WiFi.BSSIDstr());
	LOG_INFO(F("WIFI: Channel: ") << WiFi.channel());
	LOG_INFO(F("WIFI: Mode: ") << wifi_mode());
	LOG_INFO(F("WIFI: RSSI: ") << WiFi.RSSI() << F("dBm"));
	LOG_INFO(F("------------ IP Info ------------"));
	LOG_INFO(F("IP: Host name: ") << WiFi.hostname());
	LOG_INFO(F("IP: IP adress: ") << WiFi.localIP().toString());
	LOG_INFO(F("IP: Subnet mask: ") << WiFi.subnetMask());
	LOG_INFO(F("IP: Gateway IP: ") << WiFi.gatewayIP().toString());
	LOG_INFO(F("IP: DNS IP: ") << WiFi.dnsIP(0).toString());
	LOG_INFO(F("IP: MAC Address: ") << WiFi.macAddress());
}

extern void generateSha256Token(char *token, const int token_len, const char *email)
{
	LOG_INFO(F("-- START -- ") << F("Generate SHA256 token from email"));

	auto x = BearSSL::HashSHA256();
	if (email != nullptr && strlen(email))
	{
		LOG_INFO(F("E-mail: ") << email);
		x.add(email, strlen(email));
	}

	randomSeed(micros());
	uint32_t salt = rand();
	LOG_INFO(F("salt: ") << salt);
	x.add(&salt, sizeof(salt));

	salt = getChipId();
	x.add(&salt, sizeof(salt));
	LOG_INFO(F("chip id: ") << salt);

	salt = ESP.getFlashChipId();
	x.add(&salt, sizeof(salt));
	LOG_INFO(F("flash id: ") << salt);
	x.end();
	unsigned char *hash = (unsigned char *)x.hash();

	static const char digits[] = "0123456789ABCDEF";

	for (int i = 0; i < x.len() && i < token_len - 1; i += 2, hash++)
	{
		token[i] = digits[*hash >> 4];
		token[i + 1] = digits[*hash & 0xF];
	}

	LOG_INFO(F("SHA256 token: ") << token);
	LOG_INFO(F("-- END --"));
}