
#include "config.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
#include <ESP8266WiFi.h>
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"
#include "porting.h"
#include "sync_time.h"

// Конвертируем значение переменных компиляции в строк
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "=" VALUE(var)

// Сохраняем конфигурацию в EEPROM
void store_config(const Settings &sett)
{
    uint16_t crc = get_checksum(sett);
    EEPROM.begin(sizeof(sett) + sizeof(crc));
    EEPROM.put(0, sett);
    EEPROM.put(sizeof(sett), crc);

    if (!EEPROM.commit())
    {
        LOG_ERROR(F("Config stored FAILED"));
    }
    else
    {
        LOG_INFO(F("Config stored OK crc=") << crc);
    }
    EEPROM.end();
}

/* Загружаем конфигурацию в EEPROM. true - успех. */
bool load_config(Settings &sett)
{
    LOG_INFO(F("Loading Config..."));
    uint16_t crc = 0;
    Settings tmp_sett = {};
    EEPROM.begin(sizeof(tmp_sett) + sizeof(crc)); //  4 до 4096 байт. с адреса 0x7b000.
    EEPROM.get(0, tmp_sett);
    EEPROM.get(sizeof(tmp_sett), crc);
    EEPROM.end();

    uint16_t calculated_crc = get_checksum(tmp_sett);
    if (crc == calculated_crc)
    {
        sett = tmp_sett;
        LOG_INFO(F("Configuration CRC ok"));

        // Для безопасной работы с буферами,  в библиотеках может не быть проверок
        sett.waterius_host[HOST_LEN - 1] = 0;
        sett.waterius_key[WATERIUS_KEY_LEN - 1] = 0;
        sett.waterius_email[EMAIL_LEN - 1] = 0;

        sett.blynk_key[BLYNK_KEY_LEN - 1] = 0;
        sett.blynk_host[HOST_LEN - 1] = 0;
        sett.blynk_email[EMAIL_LEN - 1] = 0;
        sett.blynk_email_title[BLYNK_EMAIL_TITLE_LEN - 1] = 0;
        sett.blynk_email_template[BLYNK_EMAIL_TEMPLATE_LEN - 1] = 0;

        sett.mqtt_host[HOST_LEN - 1] = 0;
        sett.mqtt_login[MQTT_LOGIN_LEN - 1] = 0;
        sett.mqtt_password[MQTT_PASSWORD_LEN - 1] = 0;
        sett.mqtt_topic[MQTT_TOPIC_LEN - 1] = 0;
        sett.mqtt_discovery_topic[MQTT_TOPIC_LEN - 1] = 0;

        sett.ntp_server[HOST_LEN - 1] = 0;

        LOG_INFO(F("--- Waterius.ru ---- "));
        LOG_INFO(F("email=") << sett.waterius_email);
        LOG_INFO(F("host=") << sett.waterius_host << F(" key=") << sett.waterius_key);
        LOG_INFO(F("wakeup min=") << sett.wakeup_per_min);

        LOG_INFO(F("--- Blynk.cc ---- "));
        LOG_INFO(F("host=") << sett.blynk_host << F(" key=") << sett.blynk_key);
        LOG_INFO(F("email=") << sett.blynk_email);

        LOG_INFO(F("--- MQTT ---- "));
        LOG_INFO(F("host=") << sett.mqtt_host << F(" port=") << sett.mqtt_port);
        LOG_INFO(F("login=") << sett.mqtt_login << F(" pass=") << sett.mqtt_password);
        LOG_INFO(F("auto discovery=") << sett.mqtt_auto_discovery);
        LOG_INFO(F("discovery topic=") << sett.mqtt_discovery_topic);

        LOG_INFO(F("--- Network ---- "));
        if (sett.ip)
        {
            LOG_INFO(F("DHCP turn off"));
            LOG_INFO(F("static_ip=") << IPAddress(sett.ip).toString());
            LOG_INFO(F("gateway=") << IPAddress(sett.gateway).toString());
            LOG_INFO(F("mask=") << IPAddress(sett.mask).toString());
        }
        else
        {
            LOG_INFO(F("DHCP is on"));
        }

        LOG_INFO(F("ntp_server=") << sett.ntp_server);

        LOG_INFO(F("--- WIFI ---- "));
        LOG_INFO(F("wifi_ssid=") << sett.wifi_ssid);
        LOG_INFO(F("wifi_channel=") << sett.wifi_channel);

        // Всегда одно и тоже будет
        LOG_INFO(F("--- Counters ---- "));
        LOG_INFO(F("channel0 start=") << sett.channel0_start << F(", impulses=") << sett.impulses0_start << F(", factor=") << sett.factor0);
        LOG_INFO(F("channel1 start=") << sett.channel1_start << F(", impulses=") << sett.impulses1_start << F(", factor=") << sett.factor1);

        LOG_INFO(F("Config succesfully loaded"));
        return true;
    }
    else
    {
        // Конфигурация не была сохранена в EEPROM, инициализируем с нуля

        LOG_INFO(F("ESP config CRC failed. Maybe first run. Init configuration."));
        LOG_INFO(F("Saved crc=") << crc << F(" calculated=") << calculated_crc);

        sett.version = CURRENT_VERSION; // для совместимости в будущем
        LOG_INFO(F("cfg version=") << sett.version);

        strncpy0(sett.waterius_host, WATERIUS_DEFAULT_DOMAIN, sizeof(WATERIUS_DEFAULT_DOMAIN));

        strncpy0(sett.blynk_host, BLYNK_DEFAULT_DOMAIN, sizeof(BLYNK_DEFAULT_DOMAIN));

        String email_title = F("Новые показания {DEVICE_NAME}");
        strncpy0(sett.blynk_email_title, email_title.c_str(), email_title.length() + 1);

        String email_template = F("Показания:<br>Холодная: {V1}м³(+{V4}л)<br>Горячая: {V0}м³ (+{V3}л)<hr>Питание: {V2}В<br>Resets: {V5}");
        strncpy0(sett.blynk_email_template, email_template.c_str(), email_template.length() + 1);

        String default_topic = String(MQTT_DEFAULT_TOPIC_PREFIX) + "/" + String(getChipId()) + "/";
        strncpy0(sett.mqtt_topic, default_topic.c_str(), default_topic.length() + 1);
        sett.mqtt_port = MQTT_DEFAULT_PORT;

        sett.mqtt_auto_discovery = MQTT_AUTO_DISCOVERY;
        String discovery_topic(DISCOVERY_TOPIC);
        strncpy0(sett.mqtt_discovery_topic, discovery_topic.c_str(), discovery_topic.length() + 1);

        strncpy0(sett.ntp_server, DEFAULT_NTP_SERVER, sizeof(DEFAULT_NTP_SERVER));

        sett.ip = 0;
        IPAddress network_gateway;
        network_gateway.fromString(DEFAULT_GATEWAY);
        sett.gateway = network_gateway;
        IPAddress network_mask;
        network_mask.fromString(DEFAULT_MASK);
        sett.mask = network_mask;

        sett.factor1 = AUTO_IMPULSE_FACTOR;
        sett.factor0 = AS_COLD_CHANNEL;

        sett.wakeup_per_min = DEFAULT_WAKEUP_PERIOD_MIN;
        sett.set_wakeup = DEFAULT_WAKEUP_PERIOD_MIN;

        // Можно задать константы при компиляции, чтобы Ватериус сразу заработал

#ifdef BLYNK_KEY
#pragma message(VAR_NAME_VALUE(BLYNK_KEY))
        String key = VALUE(BLYNK_KEY);
        strncpy0(sett.blynk_key, key.c_str(), BLYNK_KEY_LEN);
        LOG_INFO(F("default Blynk key=") << key);
#endif

#ifdef WATERIUS_HOST
#pragma message(VAR_NAME_VALUE(WATERIUS_HOST))
        String waterius_host = VALUE(WATERIUS_HOST);
        strncpy0(sett.waterius_host, waterius_host.c_str(), WATERIUS_HOST_LEN);
        LOG_INFO("default waterius_host=" << waterius_host);
#endif

#ifdef MQTT_HOST
#pragma message(VAR_NAME_VALUE(MQTT_HOST))
        String mqtt_host = VALUE(MQTT_HOST);
        strncpy0(sett.mqtt_host, mqtt_host.c_str(), HOST_LEN);
        LOG_INFO("default mqtt_host=" << mqtt_host);
#endif

#ifdef MQTT_LOGIN
#pragma message(VAR_NAME_VALUE(MQTT_LOGIN))
        String mqtt_login = VALUE(MQTT_LOGIN);
        strncpy0(sett.mqtt_login, mqtt_login.c_str(), MQTT_LOGIN_LEN);
        LOG_INFO("default mqtt_login=" << mqtt_login);
#endif

#ifdef MQTT_PASSWORD
#pragma message(VAR_NAME_VALUE(MQTT_PASSWORD))
        String mqtt_password = VALUE(MQTT_PASSWORD);
        strncpy0(sett.mqtt_password, mqtt_password.c_str(), MQTT_PASSWORD_LEN);
        LOG_INFO("default mqtt_password=" << mqtt_password);
#endif

#ifdef WATERIUS_EMAIL
#pragma message(VAR_NAME_VALUE(WATERIUS_EMAIL))
        strncpy0(sett.waterius_email, VALUE(WATERIUS_EMAIL), EMAIL_LEN);
        LOG_INFO(F("default waterius email=") << VALUE(WATERIUS_EMAIL));
#endif

#ifdef WATERIUS_KEY
#pragma message(VAR_NAME_VALUE(WATERIUS_KEY))
        strncpy0(sett.waterius_key, VALUE(WATERIUS_KEY), WATERIUS_KEY_LEN);
        LOG_INFO(F("default waterius key=") << VALUE(WATERIUS_KEY));
#else
        LOG_INFO(F("Generate waterius key"));
        generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, sett.waterius_email);
        LOG_INFO(F("waterius key=") << sett.waterius_key);
#endif

#ifdef WIFI_SSID
#ifdef WIFI_PASS
        strncpy0(sett.wifi_ssid, VALUE(WIFI_SSID), WIFI_SSID_LEN);
        strncpy0(sett.wifi_password, VALUE(WIFI_PASS), WIFI_PWD_LEN);
        return true;
#endif
#endif
        return false;
    }
}

/*
Берем начальные показания и кол-во импульсов,
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    LOG_INFO(F("Calculating values..."));
    LOG_INFO(F("new impulses=") << data.impulses0 << " " << data.impulses1);

    if ((sett.factor1 > 0) && (sett.factor0 > 0))
    {
        cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.factor0;
        cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.factor1;
        LOG_INFO(F("new value0=") << cdata.channel0 << F(" value1=") << cdata.channel1);

        cdata.delta0 = (data.impulses0 - sett.impulses0_previous) * sett.factor0;
        cdata.delta1 = (data.impulses1 - sett.impulses1_previous) * sett.factor1;
        LOG_INFO(F("delta0=") << cdata.delta0 << F(" delta1=") << cdata.delta1);
    }
}

/* Обновляем значения в конфиге*/
void update_config(Settings &sett, const SlaveData &data, const CalculatedData &cdata)
{
    LOG_INFO(F("Updating config..."));
    // Сохраним текущие значения в памяти.
    sett.impulses0_previous = data.impulses0;
    sett.impulses1_previous = data.impulses1;

    // Перешлем время на сервер при след. включении
    sett.wake_time = millis();

    time_t now = time(nullptr);

    // Перерасчет времени пробуждения
    if (sett.mode == TRANSMIT_MODE)
    {
        // нужно удстовериться что время было устанаовлено в прошлом и сейчас
        //  т.е. перерасчет можно делать только если оба установлены или оба не установлены
        //  т.е. time должно быть или 1970 или настоящее в обоих случаях
        //  иначе коэффициенты будут расчитаны неправильно
        if (is_valid_time(now) == is_valid_time(sett.last_send))
        {
            time_t t1 = (now - sett.last_send) / 60;
            if (t1 > 1 && data.version >= 24)
            {
                LOG_INFO(F("Minutes diff:") << t1);
                sett.set_wakeup = sett.wakeup_per_min * sett.set_wakeup / t1;
                return;
            }
        }
    }
    sett.set_wakeup = sett.wakeup_per_min;
    sett.last_send = now;
}
