#include "discovery_entity.h"
#include <ArduinoJson.h>
#include "Logging.h"
#include "utils.h"
#include "publish.h"
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
 * @param entity_type тип сенсора sensor, number и т.д.
 * @param entity_name название сенсора
 * @param entity_id идентификатор сенсора
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
String build_entity_discovery(const char *mqtt_topic,
                              const char *entity_type,
                              const char *entity_name,
                              const char *entity_id,
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
    DynamicJsonDocument json_doc(JSON_DYNAMIC_MSG_BUFFER);
    JsonObject entity = json_doc.to<JsonObject>();

    entity[F("name")] = entity_name; // name

    String uniqueId_prefix = get_device_name();
    String unique_id = uniqueId_prefix + "-" + entity_id;
    entity[F("uniq_id")] = unique_id.c_str(); // unique_id

    entity[F("obj_id")] = unique_id.c_str(); // object_id

    entity[F("stat_t")] = mqtt_topic; // state_topic

    String value_template;
    value_template = String("{{ value_json.") + entity_id + String(" | is_defined }}");
    entity[F("val_tpl")] = value_template.c_str();

    if (state_class[0])
        entity[F("stat_cla")] = state_class; // state_class https://developers.home-assistant.io/docs/core/entity/sensor/#available-state-classes

    if (device_class[0])
        entity[F("dev_cla")] = device_class; // device_class

    if (unit_of_meas[0])
        entity[F("unit_of_meas")] = unit_of_meas; // unit_of_measurement

    if (entity_category[0])
        entity[F("ent_cat")] = entity_category; // entity_category

    if (icon[0])
        entity[F("ic")] = icon; // icon

    if (enabled_by_default)
        entity[F("en")] = enabled_by_default; // enabled_by_default

    if (MQTT_FORCE_UPDATE)
        entity[F("force_update")] = true; // force_update

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

    entity[F("device")] = device; // device //dv

    if (json_attributes_topic[0] && json_attributes_template[0])
    {
        entity[F("json_attributes_topic")] = json_attributes_topic;
        entity[F("json_attributes_template")] = json_attributes_template;
    }

    if (strcmp(entity_type, "number") == 0)  // << Тут нужно добавить max min step... для коректной установки значений
    {
        // https://www.home-assistant.io/integrations/number.mqtt
        String command_topic = String(mqtt_topic) + F("/") + entity_id + F("/set");
        entity[F("cmd_t")] = command_topic; // command_topic

        entity[F("cmd_tpl")] = F("{{value | round(0) | int}}"); // command_template

        entity[F("mode")] = F("box"); // mode "box"

        entity[F("min")] = 1;     // min
        entity[F("max")] = 65535; // max
        entity[F("step")] = 1;    // step

        entity[F("optimistic")] = true; // optimistic
        entity[F("retain")] = true; //retain
        entity[F("qos")] = 1; //qos
    }

    if (strcmp(entity_type, "select") == 0) // пока что ввиде грабли =(
    {
        // https://www.home-assistant.io/integrations/number.mqtt
        String command_topic = String(mqtt_topic) + F("/") + entity_id + F("/set");
        entity[F("cmd_t")] = command_topic; // command_topic

       // entity[F("cmd_tpl")] = F("{{value | round(0) | int}}"); // command_template

        //entity[F("mode")] = F("box"); // mode "box"

        //entity[F("min")] = 1;     // min
        //entity[F("max")] = 65535; // max
        //entity[F("step")] = 1;    // step

        //"options": ["WATER_COLD","WATER_HOT","ELECTRO","GAS","HEAT","PORTABLE_WATER","OTHER"],
        entity[F("options")] = F("['WATER_COLD','WATER_HOT','ELECTRO','GAS','HEAT','PORTABLE_WATER','OTHER']");

        JsonArray options = json_doc.createNestedArray("options");
        options.add("WATER_COLD");
        options.add("WATER_HOT");
        options.add("ELECTRO");
        options.add("GAS");
        options.add("HEAT");
        options.add("PORTABLE_WATER");
        options.add("OTHER");

        //"value_template": "{% set values = { \"0\":\"WATER_COLD\", \"1\":\"WATER_HOT\", \"2\":\"ELECTRO\", \"3\":\"GAS\", \"4\":\"HEAT\", \"5\":\"PORTABLE_WATER\", \"6\": \"OTHER\" } %} {{ values[ value_json.cname0 ] if value_json.cname0 in values.keys() else \"6\" }}",
        //String value_template = String(F("{% set values = { '0':\"WATER_COLD\", '1':\"WATER_HOT\", '2':\"ELECTRO\", '3':\"GAS\", '4':\"HEAT\", '5':\"PORTABLE_WATER\", '6': \"OTHER\" } %} {{ values[ value_json.")) + entity_id + F(" ] if value_json.") + entity_id + F(" in values.keys() else '6' }}");
        String value_template = String("") + 
            F("{% if value_json.")   + entity_id + F("==0 %} WATER_COLD ") +
            F("{% elif value_json.") + entity_id + F("==1 %} WATER_HOT ") +
            F("{% elif value_json.") + entity_id + F("==2 %} ELECTRO ") +
            F("{% elif value_json.") + entity_id + F("==3 %} GAS ") +
            F("{% elif value_json.") + entity_id + F("==4 %} HEAT ") +
            F("{% elif value_json.") + entity_id + F("==5 %} PORTABLE_WATER ") +
            F("{% elif value_json.") + entity_id + F("==6 %} OTHER ") + 
            F("{% endif %}");

        entity[F("val_tpl")] = value_template;

        //"command_template": "{% set values = { \"WATER_COLD\":0, \"WATER_HOT\":1,  \"ELECTRO\":2, \"GAS\":3, \"HEAT\":4, \"PORTABLE_WATER\":5, \"OTHER\":6} %}  {{ values[value] if value in values.keys() else 6 }}",
        String cmd_tpl = F("{% set values = { \"WATER_COLD\":0, \"WATER_HOT\":1, \"ELECTRO\":2, \"GAS\":3, \"HEAT\":4, \"PORTABLE_WATER\":5, \"OTHER\":6} %}  {{ values[value] if value in values.keys() else 6 }}");
        entity[F("cmd_tpl")] = cmd_tpl;

        entity[F("optimistic")] = true; // optimistic
        entity[F("retain")] = true; //retain
        entity[F("qos")] = 1; //qos
    }

    LOG_INFO(F("MQTT: DISCOVERY SENSOR: JSON Mem usage: ") << json_doc.memoryUsage());
    LOG_INFO(F("MQTT: DISCOVERY SENSOR: JSON size: ") << measureJson(json_doc));

    String payload;
    serializeJson(entity, payload);

    return payload;
}

/**
 * @brief Формирует шаблон для атрибутов сенсора
 *
 * @param attrs массив атрибутов
 * @param index индекс первого атрибута в масссиве
 * @param count количестов атрибутов в масссиве
 * @param channel канал
 * @param channel_name enum типа канала из интерфейса настройки
 * @return String строка с шаблоном для извлечения атрибутов для сенсора
 */
String get_attributes_template(const char *const attrs[][MQTT_PARAM_COUNT], int index, int count, int channel, int channel_name)
{
    String json_attributes_template = "";

    DynamicJsonDocument json_doc(JSON_DYNAMIC_MSG_BUFFER);
    JsonObject json_attributes = json_doc.to<JsonObject>();
    String attribute_name;
    String attribute_id;
    String attribute_template;

    for (int i = 0; i < count; i++)
    {
        attribute_name = FPSTR(attrs[index + i][1]);
        attribute_id = FPSTR(attrs[index + i][2]);

        update_channel_names(channel, channel_name, attribute_id, attribute_name);

        attribute_template = String(F("{{ value_json.")) + attribute_id + F(" | is_defined }}");

        json_attributes[attribute_name] = attribute_template;
    }

    serializeJson(json_attributes, json_attributes_template);
    return json_attributes_template;
}

/**
 * @brief Изменяет названия и идентификаторы сенсоров с учетом названия и идентификатора канала
 *
 * @param channel номер канала
 * @param channel_name enum типа канала из интерфейса настройки
 * @param entity_id идентификатор сенсора
 * @param entity_name имя сенсора
 */
void update_channel_names(int channel, int channel_name, String &entity_id, String &entity_name)
{
    if (channel != NONE && channel_name != NONE)
    {
        entity_id += channel;
        entity_name = String(FPSTR(CHANNEL_NAMES[channel_name])) + " " + entity_name;
    }
}
