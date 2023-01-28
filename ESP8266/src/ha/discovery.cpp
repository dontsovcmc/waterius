#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "json.h"
#include "resources.h"
#include "discovery.h"
#include "sensors.h"
#include "porting.h"

/**
 * @brief Публикация общих сведений устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
  * @param device_id идентификатор устройства
 * @param device_mac мак адрес устройства
 */
void publish_discovery_general_sensors(PubSubClient &mqtt_client, String &topic, const SlaveData &data, String &device_id, String &device_mac)
{
    String device_name = (String)WiFi.hostname();
    String device_model = FPSTR(MODEL_NAMES[data.model]);
    String sw_version = String(FIRMWARE_VERSION) + "." + String(data.version); // ESP_VERSION.ATTINY_VERSION
    String hw_version = String(HARDWARE_VERSION);                              // в дальнейшем можно модифицировать для гибкого определения версии hw
    String device_manufacturer = String(MANUFACTURER);

    // Общие сенсоры устройства
    int num_general_sensors = sizeof(GENERAL_SENSORS) / sizeof(GENERAL_SENSORS[0]);
    for (int i = 0; i < num_general_sensors; i++)
    {
        // загружаем строки из флэша
        String sensor_type = FPSTR(GENERAL_SENSORS[i][0]);
        String sensor_name = FPSTR(GENERAL_SENSORS[i][1]);
        String sensor_id = FPSTR(GENERAL_SENSORS[i][2]);
        String state_class = FPSTR(GENERAL_SENSORS[i][3]);
        String device_class = FPSTR(GENERAL_SENSORS[i][4]);
        String unit_of_meas = FPSTR(GENERAL_SENSORS[i][5]);
        String entity_category = FPSTR(GENERAL_SENSORS[i][6]);
        String icon = FPSTR(GENERAL_SENSORS[i][7]);

        if (i == 0)
        {
            // достаточно только в одном сенсоре дать полную информацию об устройстве
            publish_sensor_discovery(mqtt_client, topic.c_str(), 
                                     sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                                     state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                     entity_category.c_str(), icon.c_str(),
                                     device_id.c_str(), device_mac.c_str(), false,
                                     device_name.c_str(), device_manufacturer.c_str(), 
                                     device_model.c_str(), sw_version.c_str(), hw_version.c_str());
        }
        else
        {
            // добавляем все остальные сенсоры устройства уже без полной информации об устройстве
            publish_sensor_discovery(mqtt_client, topic.c_str(), 
                                     sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                                     state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                     entity_category.c_str(), icon.c_str(),
                                     device_id.c_str(), device_mac.c_str());
        }
    }
}


/**
 * @brief Публикация сведений устройства по каналам
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 * @param device_id идентификатор устройства
 * @param device_mac мак адрес устройства
 */
void publish_discovery_channel_sensors(PubSubClient &mqtt_client, String &topic, const SlaveData &data, String &device_id, String &device_mac)
{
    // Сенсоры по каналам, холодная и горячая вода
    for (int channel = 0; channel < CHANNEL_NUM; channel++)
    {
    
    // данные публикуются в один топик
    // создаются автодискавери топики тоолько для каждого канала
    // отстальные атрибуты канала передаются через json_attributes_topic и json_attributes_template
    String sensor_type = FPSTR(CHANNEL_SENSORS[0][0]);
    String sensor_name = String(FPSTR(CHANNEL_NAMES[channel])) + " " + String(FPSTR(CHANNEL_SENSORS[0][1]));
    String sensor_id = String(FPSTR(CHANNEL_SENSORS[0][2])) + channel;
    String state_class = FPSTR(CHANNEL_SENSORS[0][3]);
    String device_class = FPSTR(CHANNEL_SENSORS[0][4]);
    String unit_of_meas = FPSTR(CHANNEL_SENSORS[0][5]);
    String entity_category = FPSTR(CHANNEL_SENSORS[0][6]);
    String icon = FPSTR(CHANNEL_SENSORS[0][7]);

    String json_attributes_template = "";
    String json_attributes_topic = topic;

    StaticJsonDocument<JSON_STATIC_MSG_BUFFER> json_doc;
    JsonObject json_attributes = json_doc.to<JsonObject>();

    int num_channel_sensors = sizeof(CHANNEL_SENSORS) / sizeof(CHANNEL_SENSORS[0]);
    // первый сенсор (total) будет как основной, остальные показания будут как его атрибуты
    for (int i = 1; i < num_channel_sensors; i++)
    {
        String attribute_name = String(FPSTR(CHANNEL_NAMES[channel])) + " " + String(FPSTR(CHANNEL_SENSORS[i][1]));
        String attribute_id = String(FPSTR(CHANNEL_SENSORS[i][2])) + channel;
        String attribute_template = String("{{ value_json.") + attribute_id + String(" | is_defined }}");

        json_attributes[attribute_name] = attribute_template;
    }

    serializeJson(json_attributes, json_attributes_template);

    publish_sensor_discovery(mqtt_client, topic.c_str(), 
                             sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                             state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                             entity_category.c_str(), icon.c_str(),
                             device_id.c_str(), device_mac.c_str(),
                             true, "", "", "", "", "",
                             json_attributes_topic.c_str(),
                             json_attributes_template.c_str());
    
    }
}

/**
 * @brief Публикация автодисковери топиков для устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic Корневой топик
 * @param data Данные измерений
 */
void publish_discovery(PubSubClient &mqtt_client, String &topic, const SlaveData &data)
{
    LOG_INFO(F("MQTT: Publish discovery topic"));
    unsigned long start_discovery = millis();

    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();

    publish_discovery_general_sensors(mqtt_client, topic, data, device_id, device_mac);
    
    publish_discovery_channel_sensors(mqtt_client, topic, data, device_id, device_mac);


    LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start_discovery << F(" milliseconds elapsed"));
}
