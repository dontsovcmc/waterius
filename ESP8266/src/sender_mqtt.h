/**
 * @file sender_mqtt.h
 * @brief содержит функцию публикации данных в MQTT
 * @version 0.1
 * @date 2023-01-20
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef SENDERMQTT_H_
#define SENDERMQTT_H_

#ifndef MQTT_DISABLED

#ifdef MQTT_SOCKET_TIMEOUT
#undef MQTT_SOCKET_TIMEOUT
#endif
#define MQTT_SOCKET_TIMEOUT 3 // в секундах

#ifdef MQTT_MAX_PACKET_SIZE
#undef MQTT_MAX_PACKET_SIZE
#endif
#define MQTT_MAX_PACKET_SIZE 256

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"
#include "json.h"
#include "ha/discovery.h"
#include "ha/publish_data.h"
#include "utils.h"


/**
 * @brief Отправляет показания на укзанный сервер MQTT и создает discovery топик HomeAssistant
 *
 * @param sett настройки
 * @param data показания
 * @param cdata расчитанные показатели
 * @param json_data json документ c показаниями
 * @param auto_discovery если true то будет добавляться топик для автоконфигурации
 *
 * @returns true если успешно отправлены данные и false если не отправлено
 */
bool send_mqtt(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data)
{
    if (!is_mqtt(sett))
    {
        LOG_INFO(F("MQTT: SKIP"));
        return false;
    }

    LOG_INFO(F("MQTT: Creating MQTT Client..."));

    WiFiClient wifi_client;
    PubSubClient mqtt_client(wifi_client);

    wifi_client.setTimeout(MQTT_SOCKET_TIMEOUT * 1000);
    mqtt_client.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqtt_client.setServer(sett.mqtt_host, sett.mqtt_port);
    mqtt_client.setSocketTimeout(MQTT_SOCKET_TIMEOUT);
    
    String client_id = get_device_name();

    const char *login = sett.mqtt_login[0] ? sett.mqtt_login : NULL;
    const char *pass = sett.mqtt_password[0] ? sett.mqtt_password : NULL;

    String mqtt_topic = sett.mqtt_topic;
    String mqtt_discovery_topic = sett.mqtt_discovery_topic;

    remove_trailing_slash(mqtt_topic);
    remove_trailing_slash(mqtt_discovery_topic);

    LOG_INFO(F("MQTT: Connecting..."));

    if (mqtt_client.connect(client_id.c_str(), login, pass))
    {

        // публикация показаний в MQTT
        publish_data(mqtt_client, mqtt_topic, json_data, sett.mqtt_auto_discovery);

        // autodiscovery после настройки и по нажатию на кнопку
        if (sett.mqtt_auto_discovery && (ALWAYS_MQTT_AUTO_DISCOVERY ||
                                         (sett.mode == SETUP_MODE) ||
                                         (sett.mode == MANUAL_TRANSMIT_MODE)))
        {
            publish_discovery(mqtt_client, mqtt_topic, mqtt_discovery_topic, data);
        }
        mqtt_client.loop();
        mqtt_client.disconnect();

        return true;
    }
    else
    {
        LOG_ERROR(F("MQTT: Connecting failed"));
    }

    return false;
}

#endif
#endif