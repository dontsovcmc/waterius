#include "publish_data.h"
#include "Logging.h"
#include "publish.h"

/**
 * @brief Пубикует показания в один топик в формате json
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 * @param retain флаг retain
 * @return true опубликовано
 */
bool publish_data_to_single_topic(PubSubClient &mqtt_client, String &topic, JsonDocument &json_data, bool retain)
{
    String payload = "";
    serializeJson(json_data, payload);
    LOG_INFO(F("MQTT: Publish src data: ") << payload);
    return publish(mqtt_client, topic, payload, retain);
}

/**
 * @brief Публикация показаний в отдельные топики
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 * @param retain флаг retain
 * @return true опубликованы все поля, false - на первом отказе
 */
bool publish_data_to_multiple_topics(PubSubClient &mqtt_client, String &topic, JsonDocument &json_data, bool retain)
{
    JsonObject root = json_data.as<JsonObject>();
    for (JsonPair p : root)
    {
        String sensor_topic = topic + "/" + p.key().c_str();
        String sensor_value = p.value().as<String>();
        if (!publish(mqtt_client, sensor_topic, sensor_value, retain))
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Публикация показаний устройства в MQTT
 *
 * @param mqtt_client клиент MQTT
 * @param topic имя топика
 * @param json_data данные в JSON
 * @param auto_discovery настроена ли интеграция с HomeAssistant
 * @param retain флаг retain
 * @return true показания опубликованы целиком
 */
bool publish_data(PubSubClient &mqtt_client, String &topic, JsonDocument &json_data, bool auto_discovery, bool retain)
{
    unsigned long start = millis();
    bool ok;

    if (auto_discovery)
    {
        LOG_INFO(F("MQTT: Publish data to single topic"));
        // в один топик если настроена интеграция HomeAssistant
        ok = publish_data_to_single_topic(mqtt_client, topic, json_data, retain);
    }
    else
    {
        LOG_INFO(F("MQTT: Publish data to multiple topics"));
        // в оотдельные топики
        ok = publish_data_to_multiple_topics(mqtt_client, topic, json_data, retain);
    }

    LOG_INFO(F("MQTT: Publish data finished. ") << millis() - start << F(" milliseconds elapsed"));
    return ok;
}