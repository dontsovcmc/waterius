#include "subscribe.h"
#include "Logging.h"
#include "publish_data.h"
#include "utils.h"

#define MQTT_MAX_TRIES 5
#define MQTT_CONNECT_DELAY 100
#define MQTT_SUBSCRIPTION_TOPIC "/#"

extern MasterI2C masterI2C;

/**
 * @brief Обновление настроек по сообщению MQTT
 *
 * @param topic топик
 * @param payload данные из топика
 * @param sett настройки
 * @param json_data данные в JSON
 */
bool update_settings(String &topic, String &payload, Settings &sett, const SlaveData &data, DynamicJsonDocument &json_data)
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
                    if (json_data.containsKey("period_min"))   //todo добавить F("")
                    {
                        json_data[F("period_min")] = period_min;
                        updated = true;
                    }
                    LOG_INFO(F("MQTT: CALLBACK: New Settings.wakeup_per_min: ") << sett.wakeup_per_min);
                }
            }
        } else if (param.equals(F("f0")))
        {
            int f0 = payload.toInt();
            if (f0 > 0)
            {
                if (sett.factor0 != f0)
                {
                    LOG_INFO(F("MQTT: CALLBACK: Old Settings.factor0: ") << sett.factor0);
                    sett.factor0 = f0;
                    if (json_data.containsKey("f0"))
                    {
                        json_data[F("f0")] = f0;
                        updated = true;
                    }
                    LOG_INFO(F("MQTT: CALLBACK: New Settings.factor0: ") << sett.factor0);
                    
                    sett.setup_time = 0;
                    LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
                }
            }
        } else if (param.equals(F("f1")))
        {
            int f1 = payload.toInt();
            if (f1 > 0)
            {
                if (sett.factor1!= f1)
                {
                    LOG_INFO(F("MQTT: CALLBACK: Old Settings.factor1: ") << sett.factor1);
                    sett.factor1 = f1;
                    if (json_data.containsKey("f1"))
                    {
                        json_data[F("f1")] = f1;
                        updated = true;
                    }
                    LOG_INFO(F("MQTT: CALLBACK: New Settings.factor1: ") << sett.factor1);
                    sett.setup_time = 0;
                    LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
                }
            }
        } else if (param.equals(F("ch0")))
        {
            float ch0 = payload.toFloat();
            if (ch0 > 0)
            {
                updated = true;
                LOG_INFO(F("MQTT: CALLBACK: Old Settings.channel0_start: ") << sett.channel0_start);
                LOG_INFO(F("MQTT: CALLBACK: Old Settings.impulses0_start: ") << sett.impulses0_start);

                sett.channel0_start = ch0;
                sett.impulses0_start = data.impulses0;

                LOG_INFO(F("MQTT: CALLBACK: New Settings.channel0_start: ") << sett.channel0_start);
                LOG_INFO(F("MQTT: CALLBACK: New Settings.impulses0_start: ") << sett.impulses0_start);

                if (json_data.containsKey("ch0"))
                {
                    json_data[F("ch0")] = ch0;
                }
                sett.setup_time = 0;
                LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
            }
        } else if (param.equals(F("ch1")))
        {
            float ch1 = payload.toFloat();
            if (ch1 > 0)
            {
                updated = true;
                LOG_INFO(F("MQTT: CALLBACK: Old Settings.channel1_start: ") << sett.channel1_start);
                LOG_INFO(F("MQTT: CALLBACK: Old Settings.impulses1_start: ") << sett.impulses1_start);

                sett.channel1_start = ch1;
                sett.impulses1_start = data.impulses1;

                LOG_INFO(F("MQTT: CALLBACK: New Settings.channel1_start: ") << sett.channel1_start);
                LOG_INFO(F("MQTT: CALLBACK: New Settings.impulses1_start: ") << sett.impulses1_start);

                if (json_data.containsKey("ch1"))
                {
                    json_data[F("ch1")] = ch1;
                }

                sett.setup_time = 0;
                LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
            }
        } else if (param.equals(F("cname0")))
        {
            int cname0 = payload.toInt();
            if (sett.counter0_name != cname0)
            {
                LOG_INFO(F("MQTT: CALLBACK: Old Settings.counter0_name: ") << sett.counter0_name);
                sett.counter0_name = cname0;
                LOG_INFO(F("MQTT: CALLBACK: New Settings.counter0_name: ") << sett.counter0_name);
                if (json_data.containsKey("cname0"))
                {
                    json_data[F("cname0")] = cname0;
                    updated = true;
                }
                if (json_data.containsKey("data_type0"))
                {
                    json_data[F("data_type0")] = (uint8_t)data_type_by_name(cname0);
                    updated = true;
                }
                sett.setup_time = 0;
                LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
            }
        } else if (param.equals(F("cname1")))
        {
            int cname1 = payload.toInt();
            if (sett.counter1_name != cname1)
            {
                LOG_INFO(F("MQTT: CALLBACK: Old Settings.counter1_name: ") << sett.counter1_name);
                sett.counter1_name = cname1;
                LOG_INFO(F("MQTT: CALLBACK: New Settings.counter1_name: ") << sett.counter1_name);
                if (json_data.containsKey("cname1"))
                {
                    json_data[F("cname1")] = cname1;
                    updated = true;
                }
                if (json_data.containsKey("data_type1"))
                {
                    json_data[F("data_type1")] = (uint8_t)data_type_by_name(cname1);
                    updated = true;
                }
                sett.setup_time = 0;
                LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
            }
        } else if (param.equals(F("itype0")))
        {
            int itype0 = payload.toInt();
            if (data.counter_type0 != itype0)
            {
                LOG_INFO(F("MQTT: CALLBACK: Old data.counter_type0: ") << data.counter_type0);

                if (masterI2C.setCountersType(itype0, data.counter_type1))
                {
                    updated = true;

                    LOG_INFO(F("MQTT: CALLBACK: New data.counter_type0: ") << itype0);
                    if (json_data.containsKey("itype0"))
                    {
                        json_data[F("itype0")] = itype0;
                        
                    }
                }
                sett.setup_time = 0;
                LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
            }
        } else if (param.equals(F("itype1")))
        {
            int itype1 = payload.toInt();
            if (data.counter_type1 != itype1)
            {
                LOG_INFO(F("MQTT: CALLBACK: Old data.counter_type1: ") << data.counter_type1);

                if (masterI2C.setCountersType(data.counter_type0, itype1))
                {
                    updated = true;

                    LOG_INFO(F("MQTT: CALLBACK: New data.counter_type1: ") << itype1);
                    if (json_data.containsKey("itype1"))
                    {
                        json_data[F("itype1")] = itype1;
                        
                    }
                }
                sett.setup_time = 0;
                LOG_INFO(F("MQTT: CALLBACK: reset Settings.setup_time: ") << sett.setup_time);
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
void mqtt_callback(Settings &sett, const SlaveData &data, DynamicJsonDocument &json_data, PubSubClient &mqtt_client, String &mqtt_topic, char *raw_topic, byte *raw_payload, unsigned int length)
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
    if (update_settings(topic, payload, sett, data, json_data))
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
        LOG_INFO(F("MQTT: Attempt #") << MQTT_MAX_TRIES - attempts + 1 << F(" from ") << MQTT_MAX_TRIES);
        if (mqtt_client.connect(client_id.c_str(), login, pass))
        {
            LOG_INFO(F("MQTT: Connected."));
            return true;
        }
        LOG_ERROR(F("MQTT: Connect failed with state ") << mqtt_client.state());
        delay(MQTT_CONNECT_DELAY);
    } while (--attempts);
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
