#include "helpers.h"
#include "Logging.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define MQTT_CHUNK_SIZE 128

/**
 * @brief Публикация топика в MQTT по частям,
 * используется в случае если очень много информации 
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 */
void publish_chunked(PubSubClient &mqtt_client, String &topic, String &payload)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);

    int len = payload.length();
    const uint8_t *buf = (const uint8_t *)payload.c_str();
    LOG_INFO(F("MQTT: Payload Size: ") << len);

    if (mqtt_client.beginPublish(topic.c_str(), len, true))
    {
        while (len > 0)
        {
            if (len >= MQTT_CHUNK_SIZE)
            {
                mqtt_client.write(buf, MQTT_CHUNK_SIZE);
                buf += MQTT_CHUNK_SIZE;
                len -= MQTT_CHUNK_SIZE;
                LOG_INFO(F("MQTT: Sended chunk size: ") << MQTT_CHUNK_SIZE);
            }
            else
            {
                mqtt_client.write(buf, len);
                LOG_INFO(F("MQTT: Sended chunk size: ") << len);
                break;
            }
        }

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
 * @brief Публикация топика в MQTT (основной метод)
 * не использует промежуточных буферов, 
 * сообщение может иметь размер больше 250 байт
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 */
void publish(PubSubClient &mqtt_client, String &topic, String &payload)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);

    unsigned int len = payload.length();
    LOG_INFO(F("MQTT: Payload Size: ") << len);

    if (mqtt_client.beginPublish(topic.c_str(), len, true))
    {
        if (mqtt_client.print(payload.c_str()) == len)
        {
            LOG_INFO(F("MQTT: Published succesfully"));
        }
        else
        {
            LOG_ERROR(F("MQTT: Publish failed"));
        }

        mqtt_client.endPublish();
    }
    else
    {
        LOG_ERROR(F("MQTT: Client not connected."));
    }
}
/**
 * @brief Публикация топика в MQTT если сообщение меньше 250 символов
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 */
void publish_simple(PubSubClient &mqtt_client, String &topic, String &payload)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);
    LOG_INFO(F("MQTT: Payload Size: ") << payload.length());
    // LOG_INFO(F("MQTT: Payload: ") << payload);

    if (mqtt_client.connected())
    {
        if (mqtt_client.publish(topic.c_str(), payload.c_str(), true))
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

