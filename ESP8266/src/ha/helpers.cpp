#include "helpers.h"
#include "Logging.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

/**
 * @brief Публикация топика в MQTT
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 */
void publish(PubSubClient &mqtt_client, String &topic, String &payload)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);
    LOG_INFO(F("MQTT: Payload Size: ") << payload.length());
    LOG_INFO(F("MQTT: Payload: ") << payload);

    if (mqtt_client.beginPublish(topic.c_str(), payload.length(), true))
    {
        mqtt_client.write((const uint8_t *)payload.c_str(), payload.length());
        if (mqtt_client.endPublish())
        {
            LOG_INFO(F("MQTT: Published succesfully"));
        }
        else
        {
            LOG_ERROR(F("MQTT: Publish failed"));
        }
    }
    else
    {
        LOG_ERROR(F("MQTT: Client not connected."));
    }
}

/**
 * @brief Очистка топика в MQTT
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 */
void clean(PubSubClient &mqtt_client, String &topic)
{
    String payload = "";
    LOG_INFO(F("MQTT: Clean Topic: ") << topic);

    if (mqtt_client.beginPublish(topic.c_str(), payload.length(), true))
    {
        mqtt_client.print(payload);
        if (mqtt_client.endPublish())
        {
            LOG_INFO(F("MQTT: Cleaned succesfully"));
        }
        else
        {
            LOG_ERROR(F("MQTT: Clean failed"));
        }
    }
    else
    {
        LOG_ERROR(F("MQTT: Client not connected."));
    }
    
}

/**
 * @brief Пубикует показания в один топик в формате json
 *
 * @param mqtt_client клиент MQQT
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
 * @param mqtt_client клиент MQQT
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
        publish(mqtt_client, sensor_topic, sensor_value);
    }
}
