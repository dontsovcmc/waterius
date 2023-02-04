#include "subscribe.h"
#include "Logging.h"

/**
 * @brief Подписка на топик устройства
 *
 * @param mqtt_client клиент MQTT
 * @param mqtt_topic топик устройства
 */
void subscribe_to_topic(PubSubClient &mqtt_client, String &mqtt_topic)
{
    // Подписываемся на сообщения для устройства
    String subscribe_topic = mqtt_topic + F("/#");
    LOG_INFO(F("MQTT: Subscribe to ") << subscribe_topic);
    mqtt_client.subscribe(subscribe_topic.c_str(), 1);
}

/**
 * @brief Подписка на изменения в топиках MQTT
 *
 * @param raw_topic топик
 * @param raw_payload данные из топика
 * @param length длина данных
 */
void mqtt_callback(Settings &sett, char *raw_topic, byte *raw_payload, unsigned int length)
{
    String topic = raw_topic;
    String payload;
    payload.reserve(length);

    LOG_INFO(F("MQTT CALLBACK: Message arrived to: ") << topic);
    LOG_INFO(F("MQTT CALLBACK: Message payload: ") << payload);

    for (unsigned int i = 0; i < length; i++)
    {
        payload[i] = (char)raw_payload[i];
    }

    // period_min
    if (topic.endsWith(F("/period_min/set")))
    {
        LOG_INFO(F("MQTT CALLBACK: Old Settings.wakeup_per_min: ") << sett.wakeup_per_min);

        int period_min = payload.toInt();
        if (period_min)
        {
            sett.wakeup_per_min = period_min;
            LOG_INFO(F("MQTT CALLBACK: New Settings.wakeup_per_min: ") << sett.wakeup_per_min);
        }
    }
}
