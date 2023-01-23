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

#define MQTT_SOCKET_TIMEOUT 5
#define MQTT_MAX_PACKET_SIZE 1024

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"
#include "json.h"
#include "ha_discovery.h"
#include "utils.h"
/**
 * @brief Пубикует показания в один топик в формате json
 * 
 * @param mqtt_client клиент MQQT
 * @param topic имя топика
 * @param json_data данные в JSON
 */
void publish_to_single_topic(PubSubClient &mqtt_client, String &topic,DynamicJsonDocument &json_data)
{
            LOG_INFO(F("MQTT: Publish data to single topic"));
            String payload = "";
            serializeJson(json_data, payload);
            publish(mqtt_client, topic, payload);

}
/**
 * @brief публикация показаний в отдельные топики 
 * 
 * @param mqtt_client клиент MQQT
 * @param topic имя топика
 * @param json_data данные в JSON
 */
void publish_to_multiple_topics(PubSubClient &mqtt_client, String &topic,DynamicJsonDocument &json_data)
{
            LOG_INFO(F("MQTT: Publish data to multiple topics"));
            JsonObject root = json_data.as<JsonObject>();
            for (JsonPair p : root)
            {
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
                String sensor_topic = topic + "/" + p.key().c_str();
                String sensor_value = p.value().as<String>();
                publish(mqtt_client, sensor_topic, sensor_value);
            }

}


/**
 * @brief Отправляет показания на укзанный сервер MQTT и создает discovery топик HomeAssistant
 *
 * @param sett настройки
 * @param data показания
 * @param cdata расчитанные показатели
 * @param json_data json документ c показаниями
 * @param single_topic если true все данные будут отправляться одним запросом в один топик в формате json
 * @param auto_discovery если true то будет добавляться топик для автоконфигурации
 *
 * @returns true если успешно отправлены данные и false если не отправлено
 */
bool send_mqtt(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data, bool single_topic = true, bool auto_discovery = true)
{
    if (!sett.mqtt_host[0])
    {
        LOG_INFO(F("MQTT: SKIP"));
        return false;
    }

    WiFiClient wifi_client;
    PubSubClient mqtt_client(wifi_client);

    mqtt_client.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqtt_client.setServer(sett.mqtt_host, sett.mqtt_port);

    yield();

    String client_id = get_device_name();

    const char *login = sett.mqtt_login[0] ? sett.mqtt_login : nullptr;
    const char *pass = sett.mqtt_password[0] ? sett.mqtt_password : nullptr;

    String topic = sett.mqtt_topic;

    // Убираем слэш в конце
    if (topic.endsWith("/"))
        topic.remove(topic.length() - 1);

    if (mqtt_client.connect(client_id.c_str(), login, pass))
    {
        unsigned long start = millis();
        if (single_topic)
        {
            publish_to_single_topic(mqtt_client, topic,json_data);
        }
        else
        {
            publish_to_multiple_topics(mqtt_client, topic,json_data);
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
                clean_discovery(mqtt_client, topic);
            }
            // Автоматическое добавления устройства в Home Assistant
            publish_discovery(mqtt_client, topic, data, single_topic);
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