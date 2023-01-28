#include <ESP8266WiFi.h>
#include "utils.h"
#include "Logging.h"
#include "time.h"
#include "porting.h"

#define NTP_CONNECT_TIMEOUT 3000UL

#define NTP_SERV1 "1.ru.pool.ntp.org"
#define NTP_SERV2 "2.ru.pool.ntp.org"
#define NTP_SERV3 "pool.ntp.org"
/**
 * @brief Устанаваливает время по указанному серверу NTP
 *
 * @param ntp_server адрес сервера
 * @return true
 * @return false
 */
bool setClock(const char *ntp_server)
{
	configTime(0, 0, ntp_server);

	LOG_INFO(F("Waiting for NTP time sync: "));
	uint32_t start = millis();
	time_t now = time(nullptr);

	while ((now < 8 * 3600 * 2) && ((millis() - start) < NTP_CONNECT_TIMEOUT))
	{
		delay(100);
		now = time(nullptr);
	}

	return millis() - start < NTP_CONNECT_TIMEOUT;
}

/**
 * @brief Устанаваливает время пока не удастся установить
 * по следующим серверам "1.ru.pool.ntp.org", "2.ru.pool.ntp.org", "pool.ntp.org"
 *
 * @return true  если успешно
 * @return false  усли завершилось ошибкой
 */
bool setClock()
{
	if (setClock(NTP_SERV1) || setClock(NTP_SERV2) || setClock(NTP_SERV3))
	{
		LOG_INFO(F("Current time: ") << get_current_time());
		return true;
	};

	LOG_ERROR(F("SetClock FAILED"));
	return false;
}

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
 * @brief Получает текущее время
 *
 * @return строка с временем в формате C
 */
String get_current_time()
{
	char buf[100];
	time_t now = time(nullptr);
	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
	// ISO8601 date time string format (2019-11-29T23:29:55+0800).
	strftime(buf, sizeof(buf), TIME_FORMAT, &timeinfo);
	return String(buf);
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
	uint32_t checksum = 0;
	uint8_t *buf = (uint8_t *)&sett;
	int len = sizeof(sett);
	len -= 2; // вычитаем саму чексуму из длины буфера
	checksum += len;

	// сумируем по 16 бит за раз
	while (len > 1)
	{
		checksum += 0xFFFF & (*buf << 8 | *(buf + 1));
		buf += 2;
		len -= 2;
	}
	// Если количество байт было нечетным то добавим последний байт к сумме
	if (len)
	{
		checksum += 0xFFFF & (*buf << 8 | 0x00);
	}

	// возвращаем только первые 16 бит и инвертируем их
	return ((uint16_t)checksum ^ 0xFFFF);
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

bool is_waterius_site(const String &url)
{
	String temp_str = url;
	temp_str.toLowerCase();
	// специально не используется WATERIUS_DEFAULT_DOMAIN
	return temp_str.startsWith(F("https://cloud.waterius.ru"));
}

bool is_blynk(const Settings &sett)
{
#ifndef BLYNK_DISABLED
	return sett.blynk_host[0] && sett.blynk_key[0];
#else
	return false;
#endif
}

bool is_mqtt(const Settings &sett)
{
#ifndef MQTT_DISABLED
	return sett.mqtt_host[0];
#else
	return false;
#endif
}
bool is_ha(const Settings &sett)
{
#ifndef MQTT_DISABLED
	return is_mqtt(sett) && sett.mqtt_auto_discovery;
#else
	return false;
#endif
}
