
#include "ha_discovery.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "json.h"
#include "porting.h"
#include "ha_resources.h"

/**
 * @brief Публикация топика в MQTT
 *
 * @param mqtt_client клиент MQTT
 * @param topic строка с топиком
 * @param payload содержимое топика
 */
void publish(PubSubClient &mqtt_client, String &topic, String &payload)
{
    if (mqtt_client.connected())
    {
        LOG_INFO(F("MQTT: Publish Topic: ") << topic);
        if (!mqtt_client.publish(topic.c_str(), payload.c_str(), true))
        {
            LOG_ERROR(F("MQTT: Publish failed") << topic << " Payload: " << payload);
        }
        mqtt_client.loop();
        yield();
    }
    else
    {
        LOG_ERROR(F("MQTT: Client not connected."));
    }
}

/**
 * @brief Создает json для автоматического добавления сенсора в HomeAssistant и отправляет его на MQTT сервер
 * см. подробнее
 * https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
 * https://developers.home-assistant.io/docs/core/entity/sensor/
 *
 * @param mqtt_client mqtt клиент
 * @param mqtt_topic корневой топик для дискавери как правило /homeassistant.
 * @param sensor_type тип сенсора sensor, number и т.д.
 * @param sensor_name название сенсора
 * @param sensor_id идентификатор сенсора
 * @param state_class
 * @param device_class https://www.home-assistant.io/docs/configuration/customizing-devices/#device-class
 * @param unit_of_meas единицы измерения
 * @param entity_category категория
 * @param icon Любая иконка из Material Design Icons https://materialdesignicons.com/. У имени должен быть префикс mdi:, например mdi:home https://materialdesignicons.com/
 * @param device_id идентификатор устройства
 * @param device_mac MAC устройства
 * @param enabled_by_default сенсор разрешен по умолчанию
 * @param device_name Имя устройства
 * @param device_manufacturer Имя производителя
 * @param device_model Модель устройства
 * @param sw_version Версия прошивки
 * @param hw_version Версия железа
 */
void publish_sensor_discovery(PubSubClient &mqtt_client, const char *mqtt_topic, bool single_topic,
                              const char *sensor_type,
                              const char *sensor_name,
                              const char *sensor_id,
                              const char *state_class,
                              const char *device_class,
                              const char *unit_of_meas,
                              const char *entity_category,
                              const char *icon,
                              const char *device_id,
                              const char *device_mac,
                              bool enabled_by_default = true,
                              const char *device_name = "",
                              const char *device_manufacturer = "",
                              const char *device_model = "",
                              const char *sw_version = "",
                              const char *hw_version = "",
                              const char *json_attributes_topic = "",
                              const char *json_attributes_template = "")
{
    yield();
    LOG_INFO(F("MQTT: DISCOVERY:  Sensor: ") << sensor_name);

    StaticJsonDocument<JSON_STATIC_MSG_BUFFER> json_doc;
    JsonObject sensor = json_doc.to<JsonObject>();

    sensor[F("name")] = FPSTR(sensor_name); // name

    String uniqueId_prefix = get_device_name();
    String unique_id = uniqueId_prefix + "-" + sensor_id;
    sensor[F("uniq_id")] = unique_id.c_str(); // unique_id

    String object_id = unique_id;
    sensor[F("obj_id")] = object_id.c_str(); // object_id

    String value_template;
    String state_topic;

    if (single_topic)
    {
        state_topic = mqtt_topic;
        value_template = String("{{ value_json.") + sensor_id + String(" | is_defined }}");
    }
    else
    {
        state_topic = String(mqtt_topic) + "/" + sensor_id;
        value_template = "{{ value_json }}"; // value_template
    }

    sensor[F("stat_t")] = state_topic.c_str(); // state_topic
    sensor[F("val_tpl")] = value_template.c_str();

    if (state_class[0])
        sensor[F("stat_cla")] = state_class; // state_class https://developers.home-assistant.io/docs/core/entity/sensor/#available-state-classes

    if (device_class[0])
        sensor[F("dev_cla")] = device_class; // device_class

    if (unit_of_meas[0])
        sensor[F("unit_of_meas")] = unit_of_meas; // unit_of_measurement

    if (entity_category[0])
        sensor[F("ent_cat")] = entity_category; // entity_category

    if (icon[0])
        sensor[F("ic")] = icon; // icon

    if (enabled_by_default)
        sensor[F("en")] = enabled_by_default; // enabled_by_default

    if (json_attributes_template[0])
    {
        sensor[F("json_attributes_topic")] = json_attributes_topic;
        sensor[F("json_attributes_template")] = json_attributes_template;
    }

    // force_update boolean (Optional, default: false)
    // Sends update events even if the value hasn’t changed. Useful if you want to have meaningful value graphs in history.
    if (MQTT_FORCE_UPDATE)
        sensor[F("force_update")] = true; // force_update

    // TODO: Добавить топики для команд

    // if (command_topic[0])
    //     sensor["cmd_t"] = command_topic; // command_topic

    StaticJsonDocument<JSON_SMALL_STATIC_MSG_BUFFER> json_device_doc;
    JsonObject device = json_device_doc.to<JsonObject>();
    JsonArray identifiers = device.createNestedArray(F("identifiers")); // identifiers //ids

    identifiers[0] = device_id;
    identifiers[1] = device_mac;

    if (device_name[0])
        device[F("name")] = device_name; // name

    if (device_manufacturer[0])
        device[F("manufacturer")] = device_manufacturer; // manufacturer //mf

    if (device_model[0])
        device[F("model")] = device_model; // model //mdl

    if (sw_version[0])
        device[F("sw_version")] = sw_version; // sw_version //sw

    if (hw_version[0])
        device[F("hw_version")] = hw_version; // hw_version //hw

    //"connections": [["mac", "02:5b:26:a8:dc:12"]]
    // device["via_device"] = BSSID;

    sensor[F("device")] = device; // device //dv

    LOG_INFO(F("MQTT: DISCOVERY: JSON Mem usage: ") << json_doc.memoryUsage());
    LOG_INFO(F("MQTT: DISCOVERY: JSON size: ") << measureJson(json_doc));

    String payload;
    serializeJson(sensor, payload);

    String topic = String(DISCOVERY_TOPIC) + "/" + sensor_type + "/" + uniqueId_prefix + "/" + sensor_id + "/config";

    publish(mqtt_client, topic, payload);
}

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
        String payload = "";
        publish(mqtt_client, topic, payload);
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

/**
 * @brief Публикация общих сведений устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 * @param single_topic публиковать в один топик
 * @param device_id идентификатор устройства
 * @param device_mac мак адрес устройства
 */
void publish_discovery_general_sensors(PubSubClient &mqtt_client, String &topic, const SlaveData &data, bool single_topic, String &device_id, String &device_mac)
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
            publish_sensor_discovery(mqtt_client, topic.c_str(), single_topic,
                                     sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                                     state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                     entity_category.c_str(), icon.c_str(),
                                     device_id.c_str(), device_mac.c_str(), false,
                                     device_name.c_str(), device_manufacturer.c_str(), device_model.c_str(), sw_version.c_str(), hw_version.c_str());
        }
        else
        {
            // добавляем все остальные сенсоры устройства уже без полной информации об устройстве
            publish_sensor_discovery(mqtt_client, topic.c_str(), single_topic,
                                     sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                                     state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                     entity_category.c_str(), icon.c_str(),
                                     device_id.c_str(), device_mac.c_str());
        }
    }
}
/**
 * @brief
 *
 * @param mqtt_client
 * @param topic
 * @param data
 * @param single_topic
 * @param device_id
 * @param device_mac
 * @param channel
 */
void publish_discovery_channel_sensors_multiple(PubSubClient &mqtt_client, String &topic, const SlaveData &data, bool single_topic, String &device_id, String &device_mac, int channel)
{
    // если  данные публикуются в разные топики (старый подход)
    // то для каждого топика нужно создавать отдельный автодискавери топик
    int num_channel_sensors = sizeof(CHANNEL_SENSORS) / sizeof(CHANNEL_SENSORS[0]);
    for (int i = 0; i < num_channel_sensors; i++)
    {
        // загружаем строки из флэша
        String sensor_type = FPSTR(CHANNEL_SENSORS[i][0]);
        String sensor_name = String(FPSTR(CHANNEL_NAMES[channel])) + " " + String(FPSTR(CHANNEL_SENSORS[i][1]));
        String sensor_id = String(FPSTR(CHANNEL_SENSORS[i][2])) + channel;
        String state_class = FPSTR(CHANNEL_SENSORS[i][3]);
        String device_class = FPSTR(CHANNEL_SENSORS[i][4]);
        String unit_of_meas = FPSTR(CHANNEL_SENSORS[i][5]);
        String entity_category = FPSTR(CHANNEL_SENSORS[i][6]);
        String icon = FPSTR(CHANNEL_SENSORS[i][7]);

        publish_sensor_discovery(mqtt_client, topic.c_str(), single_topic,
                                 sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                                 state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                 entity_category.c_str(), icon.c_str(),
                                 device_id.c_str(), device_mac.c_str());
    }
}

//  "json_attributes_template": "{\"ADC1\": \"{{value_json.adc1}}\", \"imp1\": \"{{value_json.imp1}}\", \"delta1\": \"{{value_json.delta1}}\", \"serial1\": \"{{value_json.serial1}}\", \"f1\": \"{{value_json.f1}}\"}",

void publish_discovery_channel_sensors_single(PubSubClient &mqtt_client, String &topic, const SlaveData &data, bool single_topic, String &device_id, String &device_mac, int channel)
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

    publish_sensor_discovery(mqtt_client, topic.c_str(), single_topic,
                             sensor_type.c_str(), sensor_name.c_str(), sensor_id.c_str(),
                             state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                             entity_category.c_str(), icon.c_str(),
                             device_id.c_str(), device_mac.c_str(),
                             true, "", "", "", "", "",
                             json_attributes_topic.c_str(),
                             json_attributes_template.c_str());
}

/**
 * @brief Публикация сведений устройства по каналам
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 * @param single_topic публиковать в один топик
 * @param device_id идентификатор устройства
 * @param device_mac мак адрес устройства
 */
void publish_discovery_channel_sensors(PubSubClient &mqtt_client, String &topic, const SlaveData &data, bool single_topic, String &device_id, String &device_mac)
{
    // Сенсоры по каналам, холодная и горячая вода
    for (int channel = 0; channel < CHANNEL_NUM; channel++)
    {
        if (single_topic)
        {
            publish_discovery_channel_sensors_single(mqtt_client, topic, data, single_topic, device_id, device_mac, channel);
        }
        else
        {
            publish_discovery_channel_sensors_multiple(mqtt_client, topic, data, single_topic, device_id, device_mac, channel);
        }
    }
}

/**
 * @brief Публикация автодисковери топиков для устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic Корневой топик
 * @param data Данные измерений
 * @param single_topic признак публиковать JSON в один топик или каждое значение в отдельный топик
 */
void publish_discovery(PubSubClient &mqtt_client, String &topic, const SlaveData &data, bool single_topic = true)
{
    LOG_INFO(F("MQTT: Publish discovery topic"));
    unsigned long start_discovery = millis();

    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();

    publish_discovery_general_sensors(mqtt_client, topic, data, single_topic, device_id, device_mac);

    publish_discovery_channel_sensors(mqtt_client, topic, data, single_topic, device_id, device_mac);

    LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start_discovery << F(" milliseconds elapsed"));
}
