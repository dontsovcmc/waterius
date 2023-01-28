#include "clean.h"
#include "resources.h"
#include "Logging.h"
#include "utils.h"
#include "helpers.h"

/**
 * @brief Очистка автодискавери топиков для общих сенсоров
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param device_name название устройства
 */
void clean_discovery_general_sensors(PubSubClient &mqtt_client, String &topic, String &device_name)
{
    // Общие сенсоры устройства
    int num_general_sensors = sizeof(GENERAL_SENSORS) / sizeof(GENERAL_SENSORS[0]);
    for (int i = 0; i < num_general_sensors; i++)
    {
        // загружаем строки из флэша
        String sensor_type = FPSTR(GENERAL_SENSORS[i][0]);
        String sensor_id = FPSTR(GENERAL_SENSORS[i][2]);

        String discovery_topic = String(DISCOVERY_TOPIC) + "/" + sensor_type + "/" + device_name + "/" + sensor_id + "/config";
        
        String payload = "";
        publish(mqtt_client, topic, payload);
    }
}

/**
 * @brief Очистка автодискавери топиков для сенсоров по каждому каналу
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param device_name название устройства
 */
void clean_discovery_channel_sensors(PubSubClient &mqtt_client, String &topic, String &device_name)
{
    int num_general_sensors = sizeof(CHANNEL_SENSORS) / sizeof(CHANNEL_SENSORS[0]);
    for (int i = 0; i < num_general_sensors; i++)
    {
        // загружаем строки из флэша
        String sensor_type = FPSTR(CHANNEL_SENSORS[i][0]);
        String sensor_id = FPSTR(CHANNEL_SENSORS[i][2]);

        String discovery_topic = String(DISCOVERY_TOPIC) + "/" + sensor_type + "/" + device_name + "/" + sensor_id + "/config";
        
        clean(mqtt_client, topic);
    }
}

/**
 * @brief Очистка автодискавери топиков
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 */
void clean_discovery(PubSubClient &mqtt_client, String &topic)
{
    LOG_INFO(F("MQTT: Clean discovery topics"));
    unsigned long start_discovery = millis();

    String device_name = get_device_name();

    clean_discovery_general_sensors(mqtt_client, topic, device_name);

    clean_discovery_channel_sensors(mqtt_client, topic, device_name);

    LOG_INFO(F("MQTT: Clean discovery topics finished: ") << millis() - start_discovery << F(" milliseconds elapsed"));
}