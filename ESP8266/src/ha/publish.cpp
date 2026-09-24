#include "publish.h"
#include "Logging.h"
#include <PubSubClient.h>

/**
 * @brief Публикация топика в MQTT
 *
 * Без промежуточного буфера: сообщение может быть больше буфера клиента.
 * QoS 0, поэтому «опубликовано» значит «записано в сокет целиком» -
 * подтверждения от брокера нет. endPublish у PubSubClient 2.8 всегда
 * возвращает 1, судить о результате по нему нельзя.
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 * @param retain флаг retain
 * @return true сообщение ушло в сокет целиком
 */
bool publish(PubSubClient &mqtt_client, const String &topic, const String &payload, bool retain)
{
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
    LOG_INFO(F("MQTT: Publish Topic: ") << topic);
    LOG_INFO(F("MQTT: Payload Size: ") << payload.length());
    LOG_DEBUG(F("MQTT: Payload: ") << payload);
    LOG_INFO(F("MQTT: Retain: ") << retain);

    const unsigned int len = payload.length();
    if (!mqtt_client.beginPublish(topic.c_str(), len, retain))
    {
        LOG_ERROR(F("MQTT: Publish failed: no connection"));
        return false;
    }

    const bool sent = mqtt_client.print(payload.c_str()) == len;
    mqtt_client.endPublish();

    if (!sent)
    {
        LOG_ERROR(F("MQTT: Publish failed"));
        return false;
    }

    LOG_INFO(F("MQTT: Published succesfully"));
    return true;
}

/**
 * @brief Снять удерживаемое сообщение с топика
 *
 * Пустая посылка с флагом retain - команда брокеру забыть удерживаемое, поэтому
 * флаг взведён всегда, независимо от настройки mqtt_retain (#409): она говорит,
 * как публиковать показания, а это служебная операция протокола.
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @return true пустое сообщение ушло
 */
bool clear_retained(PubSubClient &mqtt_client, const String &topic)
{
    LOG_INFO(F("MQTT: Remove retain message: ") << topic);

    if (!mqtt_client.publish(topic.c_str(), "", true))
    {
        LOG_ERROR(F("MQTT: Publish failed"));
        return false;
    }
    return true;
}
