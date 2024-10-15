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
#define MQTT_DELAY_SUBSCRIPTION 100 // задержка для получения данных по подписке

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#endif
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"
#include "json.h"
#include "ha/publish_data.h"
#include "ha/publish_discovery.h"
#include "ha/subscribe.h"
#include "utils.h"

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

bool connect_and_subscribe_mqtt(Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data)
{
    String mqtt_topic = sett.mqtt_topic;
    remove_trailing_slash(mqtt_topic);

    wifi_client.setTimeout(MQTT_SOCKET_TIMEOUT * 1000);
    mqtt_client.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqtt_client.setServer(sett.mqtt_host, sett.mqtt_port);
    mqtt_client.setSocketTimeout(MQTT_SOCKET_TIMEOUT);

    if (sett.mqtt_auto_discovery)
    {
        // устанавливаем callback для приема команд
        // так как нужно будет изменять настройки устройства в функцие
        // то используем лямбду чтобы передать туда настройки
        // парамтеры в лямбду передаются "by reference"

        mqtt_client.setCallback([&](char *raw_topic, byte *raw_payload, unsigned int length)
                                { mqtt_callback(sett, data, json_data, mqtt_client, mqtt_topic, raw_topic, raw_payload, length); });
    }

    if (mqtt_connect(sett, mqtt_client))
    {
        if (sett.mqtt_auto_discovery)
        {
            mqtt_subscribe(mqtt_client, mqtt_topic);
            mqtt_client.loop();
        }
    }
    else
    {
        LOG_ERROR(F("MQTT: Connecting failed"));
        return false;
    }
    return true;
}

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
bool send_mqtt(Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data)
{
    unsigned long start_time = millis();
    String mqtt_topic = sett.mqtt_topic;
    remove_trailing_slash(mqtt_topic);

    if (!mqtt_client.connected())
    {
        LOG_ERROR(F("MQTT: Not connected"));
        return false;
    }

    mqtt_client.loop();
    // autodiscovery после настройки и по нажатию на кнопку
    if (sett.mqtt_auto_discovery && ((sett.mode == SETUP_MODE) ||
                                     (sett.mode == MANUAL_TRANSMIT_MODE)))
    {
        String mqtt_discovery_topic = sett.mqtt_discovery_topic;
        remove_trailing_slash(mqtt_discovery_topic);
        publish_discovery(mqtt_client, mqtt_topic, mqtt_discovery_topic, data, sett);
        mqtt_client.loop();
    }

    // публикация показаний в MQTT
    publish_data(mqtt_client, mqtt_topic, json_data, sett.mqtt_auto_discovery);

    mqtt_client.loop();
    mqtt_unsubscribe(mqtt_client, mqtt_topic);

    mqtt_client.loop();
    mqtt_client.disconnect();

    LOG_INFO(F("MQTT: Disconnect. ") << millis() - start_time << F(" milliseconds elapsed"));
    return true;
}

#endif
#endif