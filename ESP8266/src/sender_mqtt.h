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

#define MQTT_SOCKET_TIMEOUT 1
#define MQTT_MAX_PACKET_SIZE 1024


#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"
#include "json.h"
#include "ha/discovery.h"
#include "ha/clean.h"
#include "ha/helpers.h"
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
bool send_mqtt(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data, bool auto_discovery = true)
{
    if (!is_mqtt(sett))
    {
        LOG_INFO(F("MQTT: SKIP"));
        return false;
    }

    WiFiClient wifi_client;
    PubSubClient mqtt_client(wifi_client);

    mqtt_client.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqtt_client.setServer(sett.mqtt_host, sett.mqtt_port);
    mqtt_client.setSocketTimeout(MQTT_SOCKET_TIMEOUT);

    yield();

    String client_id = get_device_name();

    const char *login = sett.mqtt_login[0] ? sett.mqtt_login : nullptr;
    const char *pass = sett.mqtt_password[0] ? sett.mqtt_password : nullptr;

    String mqtt_topic = sett.mqtt_topic;

    // Убираем слэш в конце
    if (mqtt_topic.endsWith(F("/"))){
        mqtt_topic.remove(mqtt_topic.length() - 1);
    }
        
    if (mqtt_client.connect(client_id.c_str(), login, pass))
    {
        unsigned long start = millis();
        if (auto_discovery)
        {
            publish_data_to_single_topic(mqtt_client, mqtt_topic,json_data);
        }
        else
        {
            publish_data_to_multiple_topics(mqtt_client, mqtt_topic,json_data);
        }
        LOG_INFO(F("MQTT: Publish data finished. ") << millis() - start << F(" milliseconds elapsed"));

        yield();

        // autodiscovery после настройки и по нажатию на кнопку
        if (auto_discovery && (ALWAYS_MQTT_AUTO_DISCOVERY ||
                               (sett.mode == SETUP_MODE) || 
                               (sett.mode == MANUAL_TRANSMIT_MODE)))
        {

            if (sett.mode == SETUP_MODE) {
                // Если находимся в режиме настройки 
                //то предварительно очищаем все предыдущие автодискавери топики
                clean_discovery(mqtt_client, mqtt_topic);
            }
            // Автоматическое добавления устройства в Home Assistant
            publish_discovery(mqtt_client, mqtt_topic, data);
        }

        mqtt_client.disconnect();

        return true;
    }
    else
    {
        LOG_ERROR(F("MQTT connect error"));
    }

    return false;
}

#endif
#endif