
#include "config.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
#include <ESP8266WiFi.h>
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"
#include "porting.h"
#include "sync_time.h"
#include "setup.h"

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
        fillmem((uint8_t*)&sett, sizeof(sett));

        sett.version = default_version; // для совместимости в будущем
        LOG_INFO(F("cfg version=") << sett.version);

        strcpy_P(&sett.waterius_host[0], default_waterius_host);
        strcpy_P(&sett.blynk_host[0], default_blynk_host);
        strcpy_P(&sett.blynk_email_title[0], default_email_title);
        strcpy_P(&sett.blynk_email_template[0], default_email_template);
        strcpy_P(&sett.blynk_email_template[0], default_email_template);
        strcpy_P(&sett.mqtt_topic[0], default_waterius_name);
        strcat_P(&sett.mqtt_topic[0], slash);
        char buf[11];
        strcat(&sett.mqtt_topic[0],utoa(getChipId(),&buf[0], 10));
        strcat_P(&sett.mqtt_topic[0], slash);
        sett.mqtt_port = default_mqtt_port;
        sett.mqtt_auto_discovery = default_mqtt_auto_discovery;
        strcpy_P(&sett.mqtt_discovery_topic[0], default_discovery_topic);
        strcpy_P(&sett.ntp_server[0], default_ntp_server);
        sett.ip = default_ip;
        sett.mask = default_mask;
        sett.gateway = default_gateway;

        sett.factor1 = default_factor1;
        sett.factor0 = default_factor1;

        sett.wakeup_per_min = default_wakeup_per_min;
        sett.set_wakeup = default_wakeup_per_min;

        strcpy_P(&sett.blynk_key[0], default_blynk_key);
        strcpy_P(&sett.mqtt_host[0], default_mqtt_host);
        strcpy_P(&sett.mqtt_login[0], default_mqtt_login);
        strcpy_P(&sett.mqtt_password[0], default_mqtt_password);
        strcpy_P(&sett.waterius_email[0], default_waterius_email);
        strcpy_P(&sett.waterius_key[0], default_waterius_key);
        strcpy_P(&sett.wifi_ssid[0], default_wifi_ssid);
        strcpy_P(&sett.wifi_password[0], default_wifi_password);
        strcpy_P(&sett.waterius_key[0], default_waterius_key);
        LOG_INFO(F("wifi_ssid=") << sett.wifi_ssid);
        LOG_INFO(F("wifi_pass=") << sett.wifi_password);        
        return strlen_P(default_wifi_ssid)>0 && strlen_P(default_wifi_password)>0;
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
        if ((is_valid_time(now) == is_valid_time(sett.last_send) && (now > sett.last_send)))
        {
            time_t t1 = (now - sett.last_send) / 60;
            if (t1 > 1 && data.version >= 24)
            {
                LOG_INFO(F("Minutes diff:") << t1);
                sett.set_wakeup = sett.wakeup_per_min * sett.set_wakeup / t1;
                sett.last_send = now;
                return;
            }
        }
    }
    sett.set_wakeup = sett.wakeup_per_min;
    sett.last_send = now;
}
