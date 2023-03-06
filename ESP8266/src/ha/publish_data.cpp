#include "publish_data.h"
#include "Logging.h"
#include "publish.h"

/**
 * @brief Пубикует показания в один топик в формате json
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 */
void publish_data_to_single_topic(PubSubClient &mqtt_client, const char *topic, const char *payload)
{
    publish(mqtt_client, topic, payload);
}

/**
 * @brief Публикация показаний в отдельные топики
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 */
void publish_data_to_multiple_topics(PubSubClient &mqtt_client, String &topic, JsonDocument &json_data)
{
    JsonObject root = json_data.as<JsonObject>();
    for (JsonPair p : root)
    {
        String sensor_topic = topic + "/" + p.key().c_str();
        String sensor_value = p.value().as<String>();
        publish(mqtt_client, sensor_topic.c_str(), sensor_value.c_str());
    }
}

/**
 * @brief Публикация показаний устройства в MQTT
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 * @param auto_discovery настроена ли интеграция с HomeAssistant
 */
void publish_data(PubSubClient &mqtt_client, String &topic, const char *json, bool auto_discovery)
{
    unsigned long start = millis();

    if (auto_discovery)
    {
        LOG_INFO(F("MQTT: Publish data to single topic"));
        // в один топик если настроена интеграция HomeAssistant
        publish_data_to_single_topic(mqtt_client, topic.c_str(), json);
    }
    else
    {
        LOG_INFO(F("MQTT: Publish data to multiple topics"));
        // в оотдельные топики
        DynamicJsonDocument json_data(JSON_DYNAMIC_MSG_BUFFER);
        deserializeJson(json_data, json);
        publish_data_to_multiple_topics(mqtt_client, topic, json_data);
    }

    LOG_INFO(F("MQTT: Publish data finished. ") << millis() - start << F(" milliseconds elapsed"));
}