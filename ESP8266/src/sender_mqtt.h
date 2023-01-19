#ifndef _SENDERMQTT_h
#define _SENDERMQTT_h

#ifdef MQTT_SOCKET_TIMEOUT
#undef MQTT_SOCKET_TIMEOUT
#endif

#define MQTT_SOCKET_TIMEOUT 5
#define MQTT_MAX_PACKET_SIZE 1024

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"
#include "json.h"
#include "home_assistant.h"
#include "utils.h"

PubSubClient mqtt_client;
static void* eClient = nullptr;

/**
 * @brief Отправляет показания на укзанный сервер MQTT и создает discovery топик HomeAssistant
 *
 * @param sett настройки
 * @param data показания
 * @param cdata расчитанные показатели
 * @param json_data json документ c показаниями
 * @param single_topic если true все данные будут отправляться одним запросом в один топик в формате json
 * @param auto_discovery если true то будет добавляться топик для автоконфигурации
 *
 * @returns true если успешно отправлены данные и false если не отправлено
 */
bool send_mqtt(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data, bool single_topic = true, bool auto_discovery = true)
{
    if (!sett.mqtt_host[0])
    {
        LOG_INFO(F("MQTT: SKIP"));
        return false;
    }

    eClient = new WiFiClient;
    mqtt_client.setClient(*(Client*)eClient);
    mqtt_client.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqtt_client.setServer(sett.mqtt_host, sett.mqtt_port);

    yield();

    String client_id = get_device_name();

    const char *login = sett.mqtt_login[0] ? sett.mqtt_login : NULL;
    const char *pass = sett.mqtt_password[0] ? sett.mqtt_password : NULL;

    String topic = sett.mqtt_topic;

    // Убираем слэш в конце
    if (topic.endsWith("/"))
        topic.remove(topic.length() - 1);

    if (mqtt_client.connect(client_id.c_str(), login, pass))
    {
        unsigned long start = millis();
        if (single_topic)
        {
            LOG_INFO(F("MQTT: Publish data to single topic"));
            String payload = "";
            serializeJson(json_data, payload);
            LOG_INFO(F("MQTT: Topic: ") << topic << F(" Payload Length: ") << payload.length() << F(" Payload: ") << payload);
            yield();
            if (!mqtt_client.publish(topic.c_str(), payload.c_str(), true))
            {
                LOG_ERROR(F("MQTT: Publish failed to ") << topic);
            }
        }
        else
        {
            LOG_INFO(F("MQTT: Publish data to multiple topics"));
            JsonObject root = json_data.as<JsonObject>();
            for (JsonPair p : root)
            {
                yield();
                String sensor_topic = topic + "/" + p.key().c_str();
                String sensor_value = p.value().as<String>();
                LOG_INFO(F("MQTT: Topic: ") << sensor_topic << F(" Value: ") << sensor_value);
                
                if (mqtt_client.connected())
                {
                    if (!mqtt_client.publish(sensor_topic.c_str(), sensor_value.c_str(), true))
                    {
                        LOG_ERROR(F("MQTT: Publish failed to ") << topic);
                    }
                }
                else
                {
                    LOG_ERROR(F("FAILED PUBLISHING. Client not connected."));
                }
            }
        }
        LOG_INFO(F("MQTT: Publish data finished. ") << millis() - start << F(" milliseconds elapsed"));

        yield();

        if (auto_discovery)
        {
            // Автоматическое добавления устройства в Home Assistant
            LOG_INFO(F("MQTT: Publish discovery topic"));
            unsigned long start_discovery = millis();
            publish_discovery(mqtt_client, topic, data, single_topic);
            LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start_discovery << F(" milliseconds elapsed"));
        }

        mqtt_client.disconnect();

        return true;
    }
    else
    {
        LOG_ERROR(F("MQTT connect error"));
    }

    return false;
}

#endif