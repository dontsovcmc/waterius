
#include "home_assistant.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "json.h"
#include "porting.h"

#define MQTT_PARAM_COUNT 8

/**
 * @brief массив с ситемными сенсорами с указанием их атрибутов в MQTT
 *
 */
const char *GENERAL_SENSORS[][MQTT_PARAM_COUNT] PROGMEM = {
    // sensor_type, sensor_name, sensor_id, state_class, device_class,unit_of_meas,entity_category,icon
    {"sensor", "Battery Voltage", "voltage", "measurement", "voltage", "V", "diagnostic", ""},                    // voltage
    {"sensor", "Voltage diff", "voltage_diff", "measurement", "voltage", "V", "diagnostic", "mdi:battery-alert"}, // Просадка напряжения voltage_diff, В
    {"sensor", "Battery low", "voltage_low", "", "battery", "", "diagnostic", "mdi:battery-alert"},               // voltage_low Батарейка разряжена >
    {"sensor", "Battery", "battery", "measurement", "battery", "%", "diagnostic", ""},                            // процент зарядки батареи
    {"sensor", "RSSI", "rssi", "measurement", "signal_strength", "dBm", "diagnostic", "mdi:wifi"},                // rssi
    {"sensor", "Resets", "resets", "measurement", "", "", "diagnostic", "mdi:cog-refresh"},                       // resets
    {"sensor", "Time", "timestamp", "", "", "", "diagnostic", "mdi:clock"},                                       // Время
    /* {"sensor", "Version", "version", "", "", "", "diagnostic", ""},  */                                        // Версия, уже будет в свойствах девайса
    {"sensor", "Router MAC", "router_mac", "", "", "", "diagnostic", ""},                                         // Мак роутера
    {"sensor", "MAC Address", "mac", "", "", "", "diagnostic", ""},                                               // Мак ESP
    /* {"sensor", "ESP ID", "esp_id", "", "", "", "diagnostic", ""}, */                                           // ESP ID, уже будет в свойствах девайса
    {"sensor", "IP", "ip", "", "", "", "diagnostic", "mdi:ip-network"},                                           // IP
    {"sensor", "Free Memory", "freemem", "", "", "", "diagnostic", "mdi:memory"},                                 // Свободная память
    {"number", "Wake up period", "period_min", "", "duration", "min", "config", "mdi:bed-clock"}                  // Настройка для автоматического добавления времени пробуждения в Home Assistant
};

/**
 * @brief массив с сенсорами для одного канала
 *
 */
const char *CHANNEL_SENSORS[][MQTT_PARAM_COUNT] PROGMEM = {
    // sensor_type, sensor_name, sensor_id, state_class, device_class,unit_of_meas,entity_category,icon
    {"sensor", "Total", "ch", "total", "water", "m³", "", ""},                         // chN Показания
    {"sensor", "Impulses", "imp", "measurement", "", "", "diagnostic", "mdi:pulse"},   // impN Количество импульсов
    {"sensor", "Delta", "delta", "measurement", "", "", "diagnostic", "mdi:delta"},    // deltaN Разница с предыдущими показаниями, л
    {"sensor", "ADC", "adc", "measurement", "", "", "diagnostic", "mdi:counter"},      // adcN Аналоговый уровень
    {"sensor", "Serial Number", "serial", "", "", "", "diagnostic", "mdi:identifier"}, // adcN Аналоговый уровень
    {"number", "Factor", "f", "", "", "", "config", "mdi:numeric"}                     // fN  Вес импульса
};

/**
 * @brief Названия каналов
 *
 */
const char *CHANNEL_NAMES[CHANNEL_NUM] PROGMEM = {"Hot Water", "Cold Water"};

/**
 * @brief Название моделей
 *
 */
const char *MODEL_NAMES[] PROGMEM = {"Classic", "4C2W"};

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
                              const char *hw_version = "")
{
    yield();
    LOG_INFO(F("MQTT DISCOVERY:  Sensor: ") << sensor_name);
    StaticJsonDocument<JSON_STATIC_MSG_BUFFER> json_doc;
    JsonObject sensor = json_doc.to<JsonObject>();

    sensor[F("name")] = sensor_name; // name

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

    // TODO: Добавить топики для команд

    // if (command_topic[0])
    //     sensor["cmd_t"] = command_topic; // command_topic

    StaticJsonDocument<JSON_STATIC_MSG_BUFFER> json_device_doc;
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

    LOG_INFO(F("MQTT DISCOVERY: JSON Mem usage: ") << json_doc.memoryUsage());
    LOG_INFO(F("MQTT DISCOVERY: JSON size: ") << measureJson(json_doc));

    String payload;
    serializeJson(sensor, payload);

    String topic = String(DISCOVERY_TOPIC) + "/" + sensor_type + "/" + uniqueId_prefix + "/" + sensor_id + "/config";

    LOG_INFO(F("MQTT DISCOVERY: Topic: ") << topic << F(" Payload: ") << payload);

    yield();
    if (mqtt_client.connected())
    {
        if (!mqtt_client.publish(topic.c_str(), payload.c_str(), true))
        {
            LOG_ERROR(F("PUBLISH MQTT DISCOVERY"));
        }
    }
    else
    {
        LOG_ERROR(F("FAILED PUBLISHING. Client not connected."));
    }
}

void publish_discovery(PubSubClient &client, String &topic, const SlaveData &data, bool single_topic = true)
{
    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();
    String device_name = (String)WiFi.hostname();
    String device_model = MODEL_NAMES[data.model];
    String sw_version = String(FIRMWARE_VERSION) + "." + String(data.version); // ESP_VERSION.ATTINY_VERSION
    String hw_version = String(HARDWARE_VERSION);                              // в дальнейшем можно модифицировать для гибкого определения версии hw
    String device_manufacturer = String(MANUFACTURER);

    // Общие сенсоры
    int num_general_sensors = sizeof(GENERAL_SENSORS) / sizeof(GENERAL_SENSORS[0]);
    // достаточно только в одном сенсоре дать полную информацию о девайсе
    publish_sensor_discovery(client, topic.c_str(), single_topic,
                             GENERAL_SENSORS[0][0], GENERAL_SENSORS[0][1], GENERAL_SENSORS[0][2],
                             GENERAL_SENSORS[0][3], GENERAL_SENSORS[0][4], GENERAL_SENSORS[0][5],
                             GENERAL_SENSORS[0][6], GENERAL_SENSORS[0][7],
                             device_id.c_str(), device_mac.c_str(), false,
                             device_name.c_str(), device_manufacturer.c_str(), device_model.c_str(), sw_version.c_str(), hw_version.c_str());

    // добавляем все остальные
    for (int i = 1; i < num_general_sensors; i++)
    {
        publish_sensor_discovery(client, topic.c_str(), single_topic,
                                 GENERAL_SENSORS[i][0], GENERAL_SENSORS[i][1], GENERAL_SENSORS[i][2],
                                 GENERAL_SENSORS[i][3], GENERAL_SENSORS[i][4], GENERAL_SENSORS[i][5],
                                 GENERAL_SENSORS[i][6], GENERAL_SENSORS[i][7],
                                 device_id.c_str(), device_mac.c_str());
    }

    // Сенсоры по каналам
    int num_channel_sensors = sizeof(CHANNEL_SENSORS) / sizeof(CHANNEL_SENSORS[0]);
    for (int channel = 0; channel < CHANNEL_NUM; channel++)
    {
        for (int i = 0; i < num_channel_sensors; i++)
        {
            String sensor_name = String(CHANNEL_NAMES[channel]) + " " + String(CHANNEL_SENSORS[i][1]);
            String sensor_id = String(CHANNEL_SENSORS[i][2]) + channel;

            publish_sensor_discovery(client, topic.c_str(), single_topic,
                                     CHANNEL_SENSORS[i][0], sensor_name.c_str(), sensor_id.c_str(),
                                     CHANNEL_SENSORS[i][3], CHANNEL_SENSORS[i][4],
                                     CHANNEL_SENSORS[i][5], CHANNEL_SENSORS[i][6], CHANNEL_SENSORS[i][7],
                                     device_id.c_str(), device_mac.c_str());
        }
    }
}
