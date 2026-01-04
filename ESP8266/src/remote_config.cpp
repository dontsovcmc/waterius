/**
 * @file remote_config.cpp
 * @brief Получение конфигурации устройства с удаленного сервера
 * @version 1.0
 * @date 2026-01-01
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "remote_config.h"
#include "Logging.h"
#include "utils.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include "ESP8266HTTPClient.h"
#include <ArduinoJson.h>

/**
 * @brief Получить конфигурацию с сервера через POST запрос к /cfg
 */
static bool fetch_config_from_server(const String &url, const char *key, JsonDocument &response_json)
{
    if (!key || key[0] == '\0')
    {
        LOG_ERROR(F("RCFG: Key is empty, skipping config fetch"));
        return false;
    }

    void *pClient = nullptr;
    HTTPClient httpClient;
    bool result = false;
    
    // Формируем URL для запроса конфигурации
    String cfg_url = url;
    if (!cfg_url.endsWith("/"))
    {
        cfg_url += "/";
    }
    cfg_url += "cfg";
    
    LOG_INFO(F("RCFG: Fetching configuration from server"));
    LOG_INFO(F("RCFG: URL: ") << cfg_url);
    
    // Формируем JSON с ключом устройства
    JsonDocument request_json;
    request_json["key"] = key;
    String payload;
    serializeJson(request_json, payload);
    LOG_INFO(F("RCFG: Request body: ") << payload);

    String proto = get_proto(url);
    LOG_INFO(F("RCFG: Protocol: ") << proto);

    // Создаем клиент
    if (proto == PROTO_HTTP)
    {
        LOG_INFO(F("RCFG: Create insecure client"));
        pClient = new WiFiClient;
    }
    else if (proto == PROTO_HTTPS)
    {
        LOG_INFO(F("RCFG: Create secure client"));
        pClient = new WiFiClientSecure;
        (*(WiFiClientSecure *)pClient).setInsecure();
    }

    // HTTP настройки
    httpClient.setTimeout(SERVER_TIMEOUT);
    httpClient.setReuse(false);

    if (httpClient.begin(*(WiFiClient *)pClient, cfg_url))
    {
        httpClient.addHeader(F("Content-Type"), F("application/json"));
        
        LOG_INFO(F("RCFG: Sending POST request"));
        int response_code = httpClient.POST(payload);
        LOG_INFO(F("RCFG: Response code: ") << response_code);
        
        if (response_code == 200)
        {
            String response_body = httpClient.getString();
            LOG_INFO(F("RCFG: Response body: ") << response_body);
            
            // Парсим JSON ответ
            DeserializationError error = deserializeJson(response_json, response_body);
            if (error)
            {
                LOG_ERROR(F("RCFG: Failed to parse JSON response: ") << error.c_str());
                result = false;
            }
            else
            {
                LOG_INFO(F("RCFG: Configuration successfully received and parsed"));
                result = true;
            }
        }
        else
        {
            String response_body = httpClient.getString();
            LOG_ERROR(F("RCFG: Server returned error code: ") << response_code);
            LOG_ERROR(F("RCFG: Response body: ") << response_body);
            result = false;
        }
        
        httpClient.end();
        (*(WiFiClient *)pClient).stop();
    }
    else
    {
        LOG_ERROR(F("RCFG: Failed to connect to server"));
    }

    // Освобождаем память клиента
    if (proto == PROTO_HTTP)
    {
        delete (WiFiClient *)pClient;
    }
    else if (proto == PROTO_HTTPS)
    {
        delete (WiFiClientSecure *)pClient;
    }

    return result;
}

/**
 * @brief Применение настроек полученных с сервера
 */
static bool apply_config_from_server(Settings &sett, const JsonDocument &config_json, const AttinyData &data, MasterI2C &masterI2C)
{
    bool config_changed = false;
    
    LOG_INFO(F("RCFG: Applying configuration from server..."));
    
    // Обработка показаний счетчика 0
    if(!config_json["channel0"].isNull()) {
        float ch0 = config_json["channel0"];
        if(ch0 >= 0 && ch0 <= 999999) {
            sett.channel0_start = ch0;
            config_changed = true;
            LOG_INFO(F("RCFG: Updated channel0_start: ") << sett.channel0_start);
        }
    }
    
    // Обработка показаний счетчика 1
    if(!config_json["channel1"].isNull()) {
        float ch1 = config_json["channel1"];
        if(ch1 >= 0 && ch1 <= 999999) {
            sett.channel1_start = ch1;
            config_changed = true;
            LOG_INFO(F("RCFG: Updated channel1_start: ") << sett.channel1_start);
        }
    }
    
    // Обработка серийного номера счетчика 0
    if(!config_json["serial0"].isNull()) {
        String serial = config_json["serial0"].as<String>();
        if(serial.length() <= SERIAL_LEN - 1) {
            strncpy0(sett.serial0, serial.c_str(), SERIAL_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated serial0: ") << sett.serial0);
        }
    }
    
    // Обработка серийного номера счетчика 1
    if(!config_json["serial1"].isNull()) {
        String serial = config_json["serial1"].as<String>();
        if(serial.length() <= SERIAL_LEN - 1) {
            strncpy0(sett.serial1, serial.c_str(), SERIAL_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated serial1: ") << sett.serial1);
        }
    }
    
    // Обработка названия счетчика 0
    if(!config_json["cname0"].isNull()) {
        uint8_t name = config_json["cname0"];
        if(name <= COUNTER_NAME_MAX) {
            sett.counter0_name = name;
            config_changed = true;
            LOG_INFO(F("RCFG: Updated counter0_name: ") << sett.counter0_name);
        }
    }
    
    // Обработка названия счетчика 1
    if(!config_json["cname1"].isNull()) {
        uint8_t name = config_json["cname1"];
        if(name <= COUNTER_NAME_MAX) {
            sett.counter1_name = name;
            config_changed = true;
            LOG_INFO(F("RCFG: Updated counter1_name: ") << sett.counter1_name);
        }
    }
    
    // Обработка веса импульса счетчика 0
    if(!config_json["factor0"].isNull()) {
        uint16_t factor = config_json["factor0"];
        if(factor >= 1 && factor <= 10000) {
            sett.factor0 = factor;
            config_changed = true;
            LOG_INFO(F("RCFG: Updated factor0: ") << sett.factor0);
        }
    }
    
    // Обработка веса импульса счетчика 1
    if(!config_json["factor1"].isNull()) {
        uint16_t factor = config_json["factor1"];
        if(factor >= 1 && factor <= 10000) {
            sett.factor1 = factor;
            config_changed = true;
            LOG_INFO(F("RCFG: Updated factor1: ") << sett.factor1);
        }
    }
    
    // Обработка импульсов счетчика 0
    if(!config_json["impulses0"].isNull()) {
        uint32_t imp = config_json["impulses0"];
        sett.impulses0_start = imp;
        sett.impulses0_previous = imp;
        config_changed = true;
        LOG_INFO(F("RCFG: Updated impulses0: ") << imp);
    }
    
    // Обработка импульсов счетчика 1
    if(!config_json["impulses1"].isNull()) {
        uint32_t imp = config_json["impulses1"];
        sett.impulses1_start = imp;
        sett.impulses1_previous = imp;
        config_changed = true;
        LOG_INFO(F("RCFG: Updated impulses1: ") << imp);
    }
    
    // Обработка типов счетчиков 0 и 1
    // setCountersType принимает оба параметра одновременно и отправляет их на Attiny
    bool counter_type0_present = !config_json["ctype0"].isNull();
    bool counter_type1_present = !config_json["ctype1"].isNull();
    
    if(counter_type0_present || counter_type1_present) {
        // Используем текущие типы счетчиков из data, если новое значение не передано
        uint8_t ctype0 = counter_type0_present ? config_json["ctype0"].as<uint8_t>() : data.counter_type0;
        uint8_t ctype1 = counter_type1_present ? config_json["ctype1"].as<uint8_t>() : data.counter_type1;
        
        // Проверка допустимых значений (как в Web UI): 0 (NAMUR), 2 (ELECTRONIC), 255 (NONE)
        bool ctype0_valid = (ctype0 == NAMUR || ctype0 == ELECTRONIC || ctype0 == NONE);
        bool ctype1_valid = (ctype1 == NAMUR || ctype1 == ELECTRONIC || ctype1 == NONE);
        
        if(ctype0_valid && ctype1_valid) {
            if(masterI2C.setCountersType(ctype0, ctype1)) {
                config_changed = true;
                if(counter_type0_present) {
                    LOG_INFO(F("RCFG: Updated counter_type0: ") << ctype0);
                }
                if(counter_type1_present) {
                    LOG_INFO(F("RCFG: Updated counter_type1: ") << ctype1);
                }
            } else {
                LOG_ERROR(F("RCFG: Failed to set counter_type (Attiny error)"));
            }
        } else {
            LOG_ERROR(F("RCFG: Invalid counter_type value (allowed: 0, 2, 255)"));
        }
    }
    
    // Обработка периода пробуждения
    if(!config_json["wakeup_per_min"].isNull()) {
        uint16_t period = config_json["wakeup_per_min"];
        if(period >= 1 && period <= 1440) {
            sett.wakeup_per_min = period;
            reset_period_min_tuned(sett);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated wakeup_per_min: ") << sett.wakeup_per_min);
        }
    }
    
    // Обработка настроек Wi-Fi
    if(!config_json["ssid"].isNull()) {
        String ssid = config_json["ssid"].as<String>();
        if(ssid.length() > 0 && ssid.length() <= WIFI_SSID_LEN - 1) {
            strncpy0(sett.wifi_ssid, ssid.c_str(), WIFI_SSID_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated SSID"));
        }
    }
    
    if(!config_json["password"].isNull()) {
        String pass = config_json["password"].as<String>();
        if(pass.length() > 0 && pass.length() <= WIFI_PWD_LEN - 1) {
            strncpy0(sett.wifi_password, pass.c_str(), WIFI_PWD_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated WiFi password"));
        }
    }
    
    // Обработка MQTT настроек
    if(!config_json["mqtt_on"].isNull()) {
        sett.mqtt_on = config_json["mqtt_on"].as<bool>();
        config_changed = true;
        LOG_INFO(F("RCFG: Updated mqtt_on: ") << sett.mqtt_on);
    }
    
    if(!config_json["mqtt_host"].isNull() && sett.mqtt_on) {
        String host = config_json["mqtt_host"].as<String>();
        if(host.length() <= HOST_LEN - 1) {
            strncpy0(sett.mqtt_host, host.c_str(), HOST_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated mqtt_host"));
        }
    }
    
    if(!config_json["mqtt_port"].isNull() && sett.mqtt_on) {
        uint16_t port = config_json["mqtt_port"];
        if(port > 0 && port <= 65535) {
            sett.mqtt_port = port;
            config_changed = true;
            LOG_INFO(F("RCFG: Updated mqtt_port: ") << sett.mqtt_port);
        }
    }
    
    if(!config_json["mqtt_login"].isNull() && sett.mqtt_on) {
        String login = config_json["mqtt_login"].as<String>();
        if(login.length() <= MQTT_LOGIN_LEN - 1) {
            strncpy0(sett.mqtt_login, login.c_str(), MQTT_LOGIN_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated mqtt_login"));
        }
    }
    
    if(!config_json["mqtt_password"].isNull() && sett.mqtt_on) {
        String pass = config_json["mqtt_password"].as<String>();
        if(pass.length() <= MQTT_PASSWORD_LEN - 1) {
            strncpy0(sett.mqtt_password, pass.c_str(), MQTT_PASSWORD_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated mqtt_password"));
        }
    }
    
    if(!config_json["mqtt_topic"].isNull() && sett.mqtt_on) {
        String topic = config_json["mqtt_topic"].as<String>();
        if(topic.length() <= MQTT_TOPIC_LEN - 1) {
            strncpy0(sett.mqtt_topic, topic.c_str(), MQTT_TOPIC_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated mqtt_topic"));
        }
    }
    
    // Обработка HTTP настроек
    if(!config_json["http_on"].isNull()) {
        sett.http_on = config_json["http_on"].as<bool>();
        config_changed = true;
        LOG_INFO(F("RCFG: Updated http_on: ") << sett.http_on);
    }
    
    if(!config_json["http_url"].isNull() && sett.http_on) {
        String url = config_json["http_url"].as<String>();
        if(url.length() <= HOST_LEN - 1) {
            strncpy0(sett.http_url, url.c_str(), HOST_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated http_url"));
        }
    }
    
    // Обработка NTP сервера
    if(!config_json["ntp_server"].isNull()) {
        String ntp = config_json["ntp_server"].as<String>();
        if(ntp.length() <= HOST_LEN - 1) {
            strncpy0(sett.ntp_server, ntp.c_str(), HOST_LEN);
            config_changed = true;
            LOG_INFO(F("RCFG: Updated ntp_server"));
        }
    }
    
    if(config_changed) {
        LOG_INFO(F("RCFG: Configuration was updated from server"));
    } else {
        LOG_INFO(F("RCFG: No configuration changes received from server"));
    }
    
    return config_changed;
}

/**
 * @brief Получить и применить конфигурацию с удаленного сервера
 */
bool fetch_and_apply_remote_config(const String &url, const char *key, Settings &sett, const AttinyData &data, MasterI2C &masterI2C)
{
    LOG_INFO(F("RCFG: Starting remote configuration fetch..."));
    
    JsonDocument config_json;
    
    // Получаем конфигурацию с сервера
    if (!fetch_config_from_server(url, key, config_json))
    {
        LOG_ERROR(F("RCFG: Failed to fetch configuration from server"));
        return false;
    }
    
    // Проверка авторизации: ключ в ответе должен совпадать с ключом устройства
    if(!config_json["key"].isNull()) {
        String server_key = config_json["key"].as<String>();
        if(server_key != String(key)) {
            LOG_ERROR(F("RCFG: Authorization failed - key mismatch!"));
            LOG_ERROR(F("RCFG: Expected key: ") << key);
            LOG_ERROR(F("RCFG: Received key: ") << server_key);
            return false;
        }
        LOG_INFO(F("RCFG: Authorization successful - key verified"));
    } else {
        LOG_ERROR(F("RCFG: Authorization failed - no key in server response"));
        return false;
    }
    
    // Применяем полученные настройки
    if (apply_config_from_server(sett, config_json, data, masterI2C))
    {
        LOG_INFO(F("RCFG: Configuration updated, saving to EEPROM..."));
        store_config(sett);
        LOG_INFO(F("RCFG: Configuration saved successfully"));
        return true;
    }
    else
    {
        LOG_INFO(F("RCFG: No configuration changes to save"));
        return false;
    }
}

