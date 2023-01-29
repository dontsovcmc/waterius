
#include "wifi_settings.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
#include <ESP8266WiFi.h>
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"
#include "WateriusHttps.h"

#include "porting.h"

// Конвертируем значение переменных компиляции в строк
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "=" VALUE(var)

/* Сохраняем конфигурацию в EEPROM */
void storeConfig(const Settings &sett)
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
bool loadConfig(Settings &sett)
{
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
                sett.waterius_host[WATERIUS_HOST_LEN - 1] = '\0';
                sett.waterius_key[WATERIUS_KEY_LEN - 1] = '\0';
                sett.waterius_email[EMAIL_LEN - 1] = '\0';

                sett.blynk_key[BLYNK_KEY_LEN - 1] = '\0';
                sett.blynk_host[BLYNK_HOST_LEN - 1] = '\0';
                sett.blynk_email[EMAIL_LEN - 1] = '\0';
                sett.blynk_email_title[BLYNK_EMAIL_TITLE_LEN - 1] = '\0';
                sett.blynk_email_template[BLYNK_EMAIL_TEMPLATE_LEN - 1] = '\0';

                sett.mqtt_host[MQTT_HOST_LEN - 1] = '\0';
                sett.mqtt_login[MQTT_LOGIN_LEN - 1] = '\0';
                sett.mqtt_password[MQTT_PASSWORD_LEN - 1] = '\0';
                sett.mqtt_topic[MQTT_TOPIC_LEN - 1] = '\0';
                sett.mqtt_discovery_topic[MQTT_TOPIC_LEN - 1] = '\0';

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
                }
                else
                {
                        LOG_INFO(F("DHCP is on"));
                }
                LOG_INFO(F("static_ip=") << IPAddress(sett.ip).toString());
                LOG_INFO(F("gateway=") << IPAddress(sett.gateway).toString());
                LOG_INFO(F("mask=") << IPAddress(sett.mask).toString());

                // Всегда одно и тоже будет
                LOG_INFO(F("--- Counters ---- "));
                LOG_INFO(F("channel0 start=") << sett.channel0_start << F(", impulses=") << sett.impulses0_start << F(", factor=") << sett.factor0);
                LOG_INFO(F("channel1 start=") << sett.channel1_start << F(", impulses=") << sett.impulses1_start << F(", factor=") << sett.factor1);

                return true;
        }
        else
        {
                // Конфигурация не была сохранена в EEPROM, инициализируем с нуля

                LOG_INFO(F("ESP config CRC failed. Maybe first run. Init configuration."));
                LOG_INFO(F("Saved crc=") << crc << F(" calculated=") << calculated_crc);

                sett.version = CURRENT_VERSION; // для совместимости в будущем
                LOG_INFO(F("cfg version=") << sett.version);

                strncpy0(sett.waterius_host, WATERIUS_DEFAULT_DOMAIN, WATERIUS_HOST_LEN);

                strncpy0(sett.blynk_host, BLYNK_DEFAULT_DOMAIN, BLYNK_HOST_LEN);

                String email_title = F("Новые показания {DEVICE_NAME}");
                strncpy0(sett.blynk_email_title, email_title.c_str(), BLYNK_EMAIL_TITLE_LEN);

                String email_template = F("Показания:<br>Холодная: {V1}м³(+{V4}л)<br>Горячая: {V0}м³ (+{V3}л)<hr>Питание: {V2}В<br>Resets: {V5}");
                strncpy0(sett.blynk_email_template, email_template.c_str(), BLYNK_EMAIL_TEMPLATE_LEN);

                String defaultTopic = String(MQTT_DEFAULT_TOPIC_PREFIX) + "/" + String(getChipId()) + "/";
                defaultTopic.toCharArray(sett.mqtt_topic,MQTT_TOPIC_LEN);
                sett.mqtt_port = MQTT_DEFAULT_PORT;

                sett.mqtt_auto_discovery = MQTT_AUTO_DISCOVERY;
                String discovery_topic(DISCOVERY_TOPIC);
                discovery_topic.toCharArray(sett.mqtt_discovery_topic,MQTT_TOPIC_LEN);

                sett.gateway = IPAddress(192, 168, 0, 1);
                sett.mask = IPAddress(255, 255, 255, 0);

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
                strncpy0(sett.mqtt_host, mqtt_host.c_str(), MQTT_HOST_LEN);
                LOG_INFO("default mqtt_host=" << mqtt_host);
#endif

#ifdef MQTT_LOGIN
#pragma message(VAR_NAME_VALUE(MQTT_LOGIN))
                String mqtt_login = VALUE(MQTT_LOGIN);
                strncpy0(sett.mqtt_login, mqtt_login.c_str(), MQTT_HOST_LEN);
                LOG_INFO("default mqtt_login=" << mqtt_login);
#endif

#ifdef MQTT_PASSWORD
#pragma message(VAR_NAME_VALUE(MQTT_PASSWORD))
                String mqtt_password = VALUE(MQTT_PASSWORD);
                strncpy0(sett.mqtt_password, mqtt_password.c_str(), MQTT_HOST_LEN);
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
                WateriusHttps::generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN,
                                                   sett.waterius_email);
#endif

#if defined(SSID_NAME)
#if defined(SSID_PASS)
#pragma message(VAR_NAME_VALUE(SSID_NAME))
#pragma message(VAR_NAME_VALUE(SSID_PASS))

                WiFi.persistent(true);                                             // begin() will save ssid, pwd to flash
                WiFi.begin(VALUE(SSID_NAME), VALUE(SSID_PASS), 0, nullptr, false); // connect=false, т.к. мы следом вызываем Wifi.begin
                WiFi.persistent(false);                                            // don't save ssid, pwd to flash in this run
                LOG_INFO(F("default ssid=") << VALUE(SSID_NAME) << F(", pwd=") << VALUE(SSID_PASS));

                return true;
#endif
#endif
                return false;
        }
}