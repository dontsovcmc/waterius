#include "publish_discovery.h"
#include "resources.h"
#include "utils.h"
#include "porting.h"
#include "Logging.h"
#include "discovery_entity.h"
#include "publish.h"
#include "json_constructor.h"

/**
 * @brief Формирование данных для публикации автодискавери топиков
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для публикации автодискавери информации
 * @param data показания
 * @param entities массив со свойствами сенсора, кол-во свойств MQTT_PARAM_COUNT
 * @param entity_indx индекс основного сенсора в массиве
 * @param extended добавлять ли к сенсору расширенную информацию об устройтсве
 * @param attrs_index индекс первого атрибута сенсора в массиве
 * @param attrs_count кол-во атрибутов (строк в массиве)
 * @param channel индекс канала при наличии
 * @param channel_name имя канала при наличии
 */
void publish_discovery_entity(PubSubClient &mqtt_client, String &topic, String &discovery_topic,
                              const SlaveData &data, String &device_id, String &device_mac,
                              const char *const entities[][MQTT_PARAM_COUNT],
                              int entity_indx,
                              bool extended = false,
                              int attrs_index = NONE,
                              int attrs_count = NONE,
                              int channel = NONE)
{
    String sw_version = "";
    String entity_type = FPSTR(entities[entity_indx][0]);
    String entity_name = FPSTR(entities[entity_indx][1]);
    String entity_id = FPSTR(entities[entity_indx][2]);
    const __FlashStringHelper *state_class = FPSTR(entities[entity_indx][3]);
    const __FlashStringHelper *device_class = FPSTR(entities[entity_indx][4]);
    const __FlashStringHelper *unit_of_meas = FPSTR(entities[entity_indx][5]);
    const __FlashStringHelper *entity_category = FPSTR(entities[entity_indx][6]);
    const __FlashStringHelper *icon = FPSTR(entities[entity_indx][7]);

    update_channel_names(channel, entity_id, entity_name);
    JsonConstructor json(1024);
    json.begin();
    json.push(F("name"), entity_name.c_str()); // name

    String uniqueId_prefix = get_device_name();
    String unique_id = uniqueId_prefix + "-" + entity_id;
    json.push(F("uniq_id"), unique_id.c_str()); // unique_id
    json.push(F("obj_id"), unique_id.c_str());  // object_id
    json.push(F("stat_t"), topic.c_str());      // state_topic

    String value_template;
    value_template = String("{{ value_json.") + entity_id + String(" | is_defined }}");
    json.push(F("val_tpl"), value_template.c_str());

    if (state_class)
        json.push(F("stat_cla"), state_class); // state_class https://developers.home-assistant.io/docs/core/entity/sensor/#available-state-classes

    if (device_class)
        json.push(F("dev_cla"), device_class); // device_class

    if (unit_of_meas)
        json.push(F("unit_of_meas"), unit_of_meas); // unit_of_measurement

    if (entity_category)
        json.push(F("ent_cat"), entity_category); // entity_category

    if (icon)
        json.push(F("ic"), icon); // icon

    if (true)
        json.push(F("en"), true); // enabled_by_default

    if (MQTT_FORCE_UPDATE)
        json.push(F("force_update"), true); // force_update

    json.beginObject(F("device"));     // device //dv
    json.beginArray(F("identifiers")); // identifiers //ids
    json.push(device_id.c_str());
    json.push(device_mac.c_str());
    json.endArray();

    if (extended)
    {
        json.push(F("name"), get_device_name().c_str());               // name
        json.push(F("manufacturer"), F(MANUFACTURER));                 // manufacturer //mf
        json.push(F("model"), FPSTR(MODEL_NAMES[data.model]));         // model //mdl
        sw_version = String(F(FIRMWARE_VERSION)) + "." + data.version; // ESP_VERSION.ATTINY_VERSION
        json.push(F("sw_version"), sw_version.c_str());                // sw_version //sw
        json.push(F("hw_version"), F(HARDWARE_VERSION));               // hw_version //hw // в дальнейшем можно модифицировать для гибкого определения версии hw
    }
    json.endObject();

    //"connections": [["mac", "02:5b:26:a8:dc:12"]]
    // device["via_device"] = BSSID;
    if ((attrs_index != NONE) && (attrs_count != NONE))
    {
        JsonConstructor json_attributes(JSON_DYNAMIC_MSG_BUFFER);

        String attribute_name;
        String attribute_id;
        String attribute_template;

        json_attributes.begin();
        for (int i = 0; i < attrs_count; i++)
        {
            attribute_name = FPSTR(entities[attrs_index + i][1]);
            attribute_id = FPSTR(entities[attrs_index + i][2]);

            update_channel_names(channel, attribute_id, attribute_name);

            attribute_template = String(F("{{ value_json.")) + attribute_id + F(" | is_defined }}");

            json_attributes.push(attribute_name.c_str(), attribute_template.c_str());
        }
        json_attributes.end();
        json.push(F("json_attributes_topic"), topic.c_str());
        json.push(F("json_attributes_template"), json_attributes.c_str());//
        //get_attributes_template(entities, attrs_index, attrs_count, channel).c_str());
    }

    if (strcmp(entity_type.c_str(), "number") == 0)
    {
        // https://www.home-assistant.io/integrations/number.mqtt
        String command_topic = String(topic) + F("/") + entity_id + F("/set");
        json.push(F("cmd_t"), command_topic.c_str()); // command_topic

        json.push(F("cmd_tpl"), F("{{value | round(0) | int}}")); // command_template

        json.push(F("mode"), F("box")); // mode "box"

        json.push(F("min"), 1);     // min
        json.push(F("max"), 65535); // max
        json.push(F("step"), 1);    // step

        json.push(F("optimistic"), true); // optimistic
        json.push(F("retain"), true);     // retain
        json.push(F("qos"), 1);           // qos
    }
    json.end();

    String entity_discovery_topic = String(discovery_topic) + "/" + entity_type + "/" + uniqueId_prefix + "/" + entity_id + "/config";
    publish(mqtt_client, entity_discovery_topic.c_str(), json.c_str());
}

/**
 * @brief Публикация общих сенсоров устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 */
void publish_discovery_general_entities(PubSubClient &mqtt_client, String &topic, String &discovery_topic, const SlaveData &data, String &device_id, String &device_mac)
{
    // добавляем одиночные сенсоры из массива GENERAL_ENTITIES с индекса 0 ("Battery Voltage") до  7 ("RSSI")
    // всего 7 сенсоров без атрибутов
    bool extended = false;
    for (int i = 0; i < 5; i++)
    {
        extended = i == 0; // в первый сенсор дописываем всю информацию про устройство
        publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, GENERAL_ENTITIES, i, extended);
    }
    // добавляем сенсор с атрибутами из массива GENERAL_ENTITIES
    // основной сенсор 5 ("Battery Voltage") атрибуты 6 (Voltage diff)
    publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, GENERAL_ENTITIES, 5, false, 6, 1);
    // основной сенсор 8 ("RSSI") атрибуты 8,9,10 (mac router, mac, ip)
    publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, GENERAL_ENTITIES, 7, false, 8, 3);
}

/**
 * @brief Публикация сведений устройства по каналам
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 */
void publish_discovery_channel_entities(PubSubClient &mqtt_client, String &topic, String &discovery_topic, const SlaveData &data, String &device_id, String &device_mac)
{
    // Сенсоры по каналам, холодная и горячая вода
    String channel_name = "";
    for (int channel = 0; channel < CHANNEL_NUM; channel++)
    {
        // один сенсор из массива CHANNEL_ENTITIES с индексом 0 ("total") будет основным
        // остальные его атрибутами с 1 по индекс 5
        publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_ENTITIES, 0, false, 1, 5, channel);
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
    LOG_INFO(F("MQTT: Publishing discovery topic"));
    unsigned long start_time = millis();

    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();

    LOG_INFO(F("MQTT: General entities"));
    publish_discovery_general_entities(mqtt_client, topic, discovery_topic, data, device_id, device_mac);

    LOG_INFO(F("MQTT: Channel entities"));
    publish_discovery_channel_entities(mqtt_client, topic, discovery_topic, data, device_id, device_mac);

    LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start_time << F(" milliseconds elapsed"));
}
