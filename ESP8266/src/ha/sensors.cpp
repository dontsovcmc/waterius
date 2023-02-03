#include "sensors.h"
#include <ArduinoJson.h>
#include "Logging.h"
#include "utils.h"
#include "helpers.h"
#include "resources.h"

/**
 * @brief Создает json для автоматического добавления сенсора в HomeAssistant и отправляет его на MQTT сервер
 * см. подробнее
 * https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
 * https://developers.home-assistant.io/docs/core/entity/sensor/
 *
 * @param mqtt_client mqtt клиент
 * @param mqtt_topic корневой топик для публикации показаний как правил waterius-XXXXXX
 * @param mqtt_discovery_topic корневой топик для дискавери как правило /homeassistant
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
void publish_sensor_discovery(PubSubClient &mqtt_client,
                              const char *mqtt_topic,
                              const char *mqtt_discovery_topic,
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
                              bool enabled_by_default,
                              const char *device_name,
                              const char *device_manufacturer,
                              const char *device_model,
                              const char *sw_version,
                              const char *hw_version,
                              const char *json_attributes_topic,
                              const char *json_attributes_template)
{
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

    state_topic = mqtt_topic;
    value_template = String("{{ value_json.") + sensor_id + String(" | is_defined }}");

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

    String topic = String(mqtt_discovery_topic) + "/" + sensor_type + "/" + uniqueId_prefix + "/" + sensor_id + "/config";

    publish(mqtt_client, topic, payload);
}
