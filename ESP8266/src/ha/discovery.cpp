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

#define NONE -1

/**
 * @brief Формирование данных для публикации автодискавери топиков
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для публикации автодискавери информации
 * @param data показания
 * @param sensors массив со свойствами сенсора, кол-во свойств MQTT_PARAM_COUNT
 * @param sensor_indx индекс основного сенсора в массиве
 * @param extended добавлять ли к сенсору расширенную информацию об устройтсве
 * @param attrs_index индекс первого атрибута сенсора в массиве
 * @param attrs_count кол-во атрибутов (строк в массиве)
 * @param channel индекс канала при наличии
 * @param channel_name имя канала при наличии
 */
void publish_sensor(PubSubClient &mqtt_client, String &topic, String &discovery_topic,
                    const SlaveData &data, String &device_id, String &device_mac,
                    const char *const sensors[][MQTT_PARAM_COUNT],
                    int sensor_indx,
                    bool extended = false,
                    int attrs_index = NONE,
                    int attrs_count = NONE,
                    int channel = NONE,
                    const char *channel_name = "")
{

    String device_name = "";
    String device_model = "";
    String sw_version = "";
    String hw_version = "";
    String device_manufacturer = "";
    String sensor_type = FPSTR(sensors[sensor_indx][0]);
    String sensor_name = FPSTR(sensors[sensor_indx][1]);
    String sensor_id = FPSTR(sensors[sensor_indx][2]);
    String state_class = FPSTR(sensors[sensor_indx][3]);
    String device_class = FPSTR(sensors[sensor_indx][4]);
    String unit_of_meas = FPSTR(sensors[sensor_indx][5]);
    String entity_category = FPSTR(sensors[sensor_indx][6]);
    String icon = FPSTR(sensors[sensor_indx][7]);
    String json_attributes_template = "";
    String json_attributes_topic = "";

    if (channel >= 0)
    {
        sensor_id += channel;
    }
    if (channel_name[0])
    {
        sensor_name = String(channel_name) + " " + sensor_name;
    }

    if (extended)
    {
        device_name = WiFi.hostname();
        device_model = FPSTR(MODEL_NAMES[data.model]);
        sw_version = String(FIRMWARE_VERSION) + "." + data.version; // ESP_VERSION.ATTINY_VERSION
        hw_version = HARDWARE_VERSION;                              // в дальнейшем можно модифицировать для гибкого определения версии hw
        device_manufacturer = MANUFACTURER;
    }
    
    if ((attrs_index != NONE) && (attrs_count != NONE))
    {
        StaticJsonDocument<JSON_STATIC_MSG_BUFFER> json_doc;
        JsonObject json_attributes = json_doc.to<JsonObject>();
        String attribute_name;
        String attribute_id;
        String attribute_template;
        
        
        for (int i = 0; i < attrs_count; i++)
        {
            attribute_name = String(FPSTR(sensors[attrs_index + i][1]));
            attribute_id = String(FPSTR(sensors[attrs_index + i][2]));

            if (channel != NONE)
            {
                attribute_id += channel;
            }
            if (channel_name[0])
            {
                attribute_name = String(channel_name) + " " + attribute_name;
            }
           
            attribute_template = String(F("{{ value_json.")) + attribute_id + String(F(" | is_defined }}"));
            
            json_attributes[attribute_name] = attribute_template;
        }

        serializeJson(json_attributes, json_attributes_template);
        json_attributes_topic = topic;

    }

    publish_sensor_discovery(mqtt_client, topic.c_str(), discovery_topic.c_str(),
                             sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                             state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                             entity_category.c_str(), icon.c_str(),
                             device_id.c_str(), device_mac.c_str(),
                             true, device_name.c_str(), device_manufacturer.c_str(),
                             device_model.c_str(), sw_version.c_str(), hw_version.c_str(),
                             json_attributes_topic.c_str(),
                             json_attributes_template.c_str());
}

/**
 * @brief Публикация общих сведений устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 */
void publish_discovery_general_sensors(PubSubClient &mqtt_client, String &topic, String &discovery_topic, const SlaveData &data, String &device_id, String &device_mac)
{
    // добавляем одиночные сенсоры из массива GENERAL_SENSORS с индекса 0 ("Battery Voltage") до  7 ("RSSI")
    // всего 7 сенсоров без атрибутов
    bool extended = false;
    for (int i = 0; i <=4; i++)
    {
        extended = i == 0; // в первый сенсор дописываем всю информацию про устройство
        publish_sensor(mqtt_client, topic, discovery_topic, data, device_id, device_mac, GENERAL_SENSORS, i, extended);
    }
    // добавляем сенсор с атрибутами из массива GENERAL_SENSORS
    // основной сенсор 5 ("Battery Voltage") атрибуты 6 (Voltage diff)
    publish_sensor(mqtt_client, topic, discovery_topic, data, device_id, device_mac, GENERAL_SENSORS, 5, false, 6, 1);
    // основной сенсор 8 ("RSSI") атрибуты 8,9,10 (mac router, mac, ip)
    publish_sensor(mqtt_client, topic, discovery_topic, data, device_id, device_mac, GENERAL_SENSORS, 7, false, 8, 3);
}

/**
 * @brief Публикация сведений устройства по каналам
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 */
void publish_discovery_channel_sensors(PubSubClient &mqtt_client, String &topic, String &discovery_topic, const SlaveData &data, String &device_id, String &device_mac)
{
    // Сенсоры по каналам, холодная и горячая вода
    String channel_name = "";
    for (int channel = 0; channel < CHANNEL_NUM; channel++)
    {
        channel_name = FPSTR(CHANNEL_NAMES[channel]);
        // один сенсор из массива CHANNEL_SENSORS с индексом 0 ("total") будет основным
        // остальные его атрибутами с 1 по индекс 4
        publish_sensor(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_SENSORS, 0, false, 1, 4, channel, channel_name.c_str());

        // добавляем одиночный сенсор из массива CHANNEL_SENSORS с индексом 5 ("freq")
        publish_sensor(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_SENSORS, 5, false, NONE, NONE, channel, channel_name.c_str());
    }
}

/**
 * @brief Публикация автодисковери топиков для устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic Корневой топик
 * @param data Данные измерений
 */
void publish_discovery(PubSubClient &mqtt_client, String &topic, String &discovery_topic, const SlaveData &data)
{
    LOG_INFO(F("MQTT: Publish discovery topic"));
    unsigned long start = millis();

    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();

    publish_discovery_general_sensors(mqtt_client, topic, discovery_topic, data, device_id, device_mac);

    publish_discovery_channel_sensors(mqtt_client, topic, discovery_topic, data, device_id, device_mac);

    LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start << F(" milliseconds elapsed"));
}
