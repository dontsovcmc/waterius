#include "publish.h"
#include "Logging.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

/**
 * @brief Публикация топика в MQTT в различных режимах
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 * @param mode режим публикации, режим по умолчанию PUBLISH_MODE_BIG
 */
void publish(PubSubClient &mqtt_client, const char *topic, const char *payload, int mode)
{
    switch (mode)
    {
    case PUBLISH_MODE_SIMPLE:
        publish_simple(mqtt_client, topic, payload);
        break;
    case PUBLISH_MODE_CHUNKED:
        publish_chunked(mqtt_client, topic, payload);
        break;
    case PUBLISH_MODE_BIG:
    default:
        publish_big(mqtt_client, topic, payload);
    }
}

/**
 * @brief Публикация топика в MQTT по частям,
 * используется в случае если очень много информации
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 */
void publish_chunked(PubSubClient &mqtt_client, const char *topic, const char *payload, unsigned int chunk_size)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);
    LOG_INFO(F("MQTT: Payload Size: ") << strlen(payload));

    unsigned int len = strlen(payload);
    const uint8_t *buf = (const uint8_t *)payload;

    if (mqtt_client.beginPublish(topic, len, true))
    {
        while (len > 0)
        {
            if (len >= chunk_size)
            {
                mqtt_client.write(buf, chunk_size);
                buf += chunk_size;
                len -= chunk_size;
                LOG_INFO(F("MQTT: Sended chunk size: ") << chunk_size);
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
void publish_big(PubSubClient &mqtt_client, const char *topic, const char *payload)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);
    LOG_INFO(F("MQTT: Payload Size: ") << strlen(payload));

    unsigned int len = strlen(payload);
    if (mqtt_client.beginPublish(topic, len, true))
    {
        if (mqtt_client.print(payload) == len)
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
void publish_simple(PubSubClient &mqtt_client, const char *topic, const char *payload)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);
    LOG_INFO(F("MQTT: Payload Size: ") << strlen(payload));

    if (mqtt_client.connected())
    {
        if (mqtt_client.publish(topic, payload, true))
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
