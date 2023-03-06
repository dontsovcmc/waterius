#include "subscribe.h"
#include "Logging.h"
#include "publish_data.h"
#include "utils.h"
#include "string.h"

#define MQTT_MAX_TRIES 5
#define MQTT_CONNECT_DELAY 100
#define MQTT_SUBSCRIPTION_TOPIC "/#"

extern "C" bool periodUpdated;

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
void mqtt_callback(Settings &sett, char *raw_topic, byte *raw_payload, unsigned int length)
{
    char *i[3] = {nullptr};
    char *topic = (char *)malloc(strlen(raw_topic)+1);
    if (!topic)
        return;
    strcpy(topic, raw_topic);
    *(topic+strlen(raw_topic))='\0';
    char *ind = strstr(topic, "/");
    while (ind)
    {
        i[2] = i[1];
        i[1] = i[0];
        *ind++ = '\0';
        i[0] = ind;
        ind = strstr(ind, "/");
    }
    LOG_INFO(F("MQTT: CALLBACK: i[0]: ") << i[0]);
    LOG_INFO(F("MQTT: CALLBACK: i[1]: ") << i[1]);
    LOG_INFO(F("MQTT: CALLBACK: i[2]: ") << i[2]);
    if ((strcmp(i[0], "set") == 0) && (strcmp(i[1], "period_min") == 0))
    {
        uint16_t period_min = atoi((char *)raw_payload);
        if (period_min > 0)
        {
            // обновили в настройках
            if (sett.wakeup_per_min != period_min)
            {
                LOG_INFO(F("MQTT: CALLBACK: Old Settings.wakeup_per_min: ") << sett.wakeup_per_min);
                sett.wakeup_per_min = period_min;
                periodUpdated = true;
                LOG_INFO(F("MQTT: CALLBACK: New Settings.wakeup_per_min: ") << sett.wakeup_per_min);
            }
        }
    }
    free(topic);
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
        LOG_INFO(F("MQTT: Attempt #") << MQTT_MAX_TRIES - attempts + 1 << F(" from ") << MQTT_MAX_TRIES);
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
