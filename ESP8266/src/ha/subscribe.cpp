#include "subscribe.h"
#include "Logging.h"
#include "publish_data.h"
#include "utils.h"

#define MQTT_MAX_TRIES 5
#define MQTT_CONNECT_DELAY 100
#define MQTT_SUBSCRIPTION_TOPIC "/#"

/**
 * @brief Обновление настроек по сообщению MQTT
 *
 * @param topic топик
 * @param payload данные из топика
 * @param sett настройки
 * @param json_data данные в JSON
 */
bool update_settings(String &topic, String &payload, Settings &sett, DynamicJsonDocument &json_data)
{
    bool updated = false;
    if (topic.endsWith(F("/set"))) // пришла команда на изменение
    {
        // извлекаем имя параметра
        int endslash = topic.lastIndexOf('/');
        int prevslash = topic.lastIndexOf('/', endslash - 1);
        String param = topic.substring(prevslash + 1, endslash);
        LOG_INFO(F("MQTT CALLBACK: Parameter ") << param);

        // period_min
        if (param.equals(F("period_min")))
        {
            int period_min = payload.toInt();
            if (period_min > 0)
            {
                // обновили в настройках
                if (sett.wakeup_per_min != period_min)
                {
                    LOG_INFO(F("MQTT: CALLBACK: Old Settings.wakeup_per_min: ") << sett.wakeup_per_min);
                    sett.wakeup_per_min = period_min;
                    // если есть ключ то время уже получено и json уже сформирован, можно отправлять
                    if (json_data.containsKey("period_min"))
                    {
                        json_data[F("period_min")] = period_min;
                        updated = true;
                    }
                    LOG_INFO(F("MQTT: CALLBACK: New Settings.wakeup_per_min: ") << sett.wakeup_per_min);
                }
            }
        }
    }
    return updated;
}

/**
 * @brief Обработка пришедшего сообщения по подписке
 *
 * @param sett настройки
 * @param mqtt_client клиент MQTT
 * @param json_data данные JSON
 * @param raw_topic топик
 * @param raw_payload  данные из топика
 * @param length длина сообщения
 */
void mqtt_callback(Settings &sett, DynamicJsonDocument &json_data, PubSubClient &mqtt_client, String &mqtt_topic, char *raw_topic, byte *raw_payload, unsigned int length)
{
    String topic = raw_topic;
    String payload;
    payload.reserve(length);

    LOG_INFO(F("MQTT: CALLBACK: Message arrived to: ") << topic);
    LOG_INFO(F("MQTT: CALLBACK: Message length: ") << length);

    for (unsigned int i = 0; i < length; i++)
    {
        payload += (char)raw_payload[i];
    }
    LOG_INFO(F("MQTT: CALLBACK: Message payload: ") << payload);
    if (update_settings(topic, payload, sett, json_data))
    {
        // если данные изменились то переопубликуем их сразу не ожидая следующего сеанса связи
        publish_data(mqtt_client, mqtt_topic, json_data, true);
    }
}

/**
 * @brief Подключается к серверу MQTT c таймаутом и несколькими попытками
 *
 * @param sett настройки
 * @param mqtt_client клиент MQTT
 * @param mqtt_topic строка с топиком
 * @return true Удалось подключиться,
 * @return false Не удалось подключиться
 */
bool mqtt_connect(Settings &sett, PubSubClient &mqtt_client)
{
    String client_id = get_device_name();
    const char *login = sett.mqtt_login[0] ? sett.mqtt_login : NULL;
    const char *pass = sett.mqtt_password[0] ? sett.mqtt_password : NULL;
    LOG_INFO(F("MQTT: Connecting..."));
    int attempts = MQTT_MAX_TRIES;
    do
    {
        LOG_INFO(F("MQTT: Tries #") << attempts << F(" from ") << MQTT_MAX_TRIES);
        if (mqtt_client.connect(client_id.c_str(), login, pass))
        {
            LOG_INFO(F("MQTT: Connected."));
            return true;
        }
        LOG_ERROR(F("MQTT: Connect failed with state ") << mqtt_client.state());
        delay(MQTT_CONNECT_DELAY);
    } while (attempts--);
    return true;
}

/**
 * @brief Подписка на все субтопики устройства
 *
 * @param mqtt_client клиент MQTT
 * @param mqtt_topic строка с топиком
 * @return true удалось подключиться,
 * @return false не удалось подключиться
 */
bool mqtt_subscribe(PubSubClient &mqtt_client, String &mqtt_topic)
{
    String subscribe_topic = mqtt_topic + F(MQTT_SUBSCRIPTION_TOPIC);
    if (!mqtt_client.subscribe(subscribe_topic.c_str(), 1))
    {
        LOG_ERROR(F("MQTT: Failed Subscribe to ") << subscribe_topic);
        return false;
    }

    LOG_INFO(F("MQTT: Subscribed to ") << subscribe_topic);

    return true;
}

/**
 * @brief Отписка от сообщений на все субтопики устройства
 *
 * @param mqtt_client
 * @param mqtt_topic
 * @return true
 * @return false
 */
bool mqtt_unsubscribe(PubSubClient &mqtt_client, String &mqtt_topic)
{
    String subscribe_topic = mqtt_topic + F(MQTT_SUBSCRIPTION_TOPIC);
    if (!mqtt_client.unsubscribe(subscribe_topic.c_str()))
    {
        LOG_ERROR(F("MQTT: Failed Unsubscribe from ") << subscribe_topic);
        return false;
    }

    LOG_INFO(F("MQTT: Unsubscribed from ") << subscribe_topic);

    return true;
}
