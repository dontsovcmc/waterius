#include "publish_data.h"
#include "Logging.h"
#include "helpers.h"

/**
 * @brief Пубикует показания в один топик в формате json
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 */
void publish_data_to_single_topic(PubSubClient &mqtt_client, String &topic, DynamicJsonDocument &json_data)
{
    LOG_INFO(F("MQTT: Publish data to single topic"));
    String payload = "";
    serializeJson(json_data, payload);
    publish(mqtt_client, topic, payload);
}

/**
 * @brief публикация показаний в отдельные топики
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 */
void publish_data_to_multiple_topics(PubSubClient &mqtt_client, String &topic, DynamicJsonDocument &json_data)
{
    LOG_INFO(F("MQTT: Publish data to multiple topics"));
    JsonObject root = json_data.as<JsonObject>();
    for (JsonPair p : root)
    {
        String sensor_topic = topic + "/" + p.key().c_str();
        String sensor_value = p.value().as<String>();
        publish_simple(mqtt_client, sensor_topic, sensor_value);
    }
}

/**
 * @brief публикация показаний устройства в MQTT
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 * @param auto_discovery настроена ли интеграция с HomeAssistant
 */
void publish_data(PubSubClient &mqtt_client, String &topic, DynamicJsonDocument &json_data, bool auto_discovery)
{
    LOG_INFO(F("MQTT: Connected."));
    
    unsigned long start = millis();

    if (auto_discovery)
    {
        // в один топик если настроена интеграция HomeAssistant
        publish_data_to_single_topic(mqtt_client, topic, json_data);
    }
    else
    {
        // в оотдельные топики
        publish_data_to_multiple_topics(mqtt_client, topic, json_data);
    }

    LOG_INFO(F("MQTT: Publish data finished. ") << millis() - start << F(" milliseconds elapsed"));
}