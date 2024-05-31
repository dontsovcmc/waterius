#include <ArduinoJson.h>
#include "publish_discovery.h"
#include "resources.h"
#include "utils.h"
#include "porting.h"
#include "Logging.h"
#include "utils.h"
#include "discovery_entity.h"
#include "publish.h"

/**
 * @brief Формирование данных для публикации автодискавери топиков
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для публикации автодискавери информации
 * @param device_id
 * @param device_mac
 * @param entity свойства сенсора
 * @param device_name Имя устройства
 * @param device_manufacturer Имя производителя
 * @param device_model Модель устройства
 * @param sw_version Версия прошивки
 * @param hw_version Версия железа
 */
void publish_discovery_entity(PubSubClient &mqtt_client, 
                              const String &topic, 
                              const String &discovery_topic,
                              const String &device_id,
                              const String &device_mac,
                              const char *const entity[MQTT_PARAM_COUNT],
                              const char *device_name = nullptr,
                              const char *device_manufacturer = nullptr,
                              const char *device_model = nullptr,
                              const char *sw_version = nullptr,
                              const char *hw_version = nullptr)
{
    String entity_type = FPSTR(entity[0]);
    String entity_name = FPSTR(entity[1]); 
    String entity_id = FPSTR(entity[2]);
    String state_class = FPSTR(entity[3]);
    String device_class = FPSTR(entity[4]);
    String unit_of_meas = FPSTR(entity[5]);
    String entity_category = FPSTR(entity[6]);
    String icon = FPSTR(entity[7]);
    String advanced_conf = FPSTR(entity[8]);

    String uniqueId_prefix = get_device_name();

    LOG_INFO(F("MQTT: DISCOVERY:  Sensor: ") << entity_name);

    String payload = build_entity_discovery(topic.c_str(),
                                            entity_type.c_str(), entity_name.c_str(), entity_id.c_str(),
                                            state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                            entity_category.c_str(), icon.c_str(),
                                            device_id.c_str(), device_mac.c_str(),
                                            true, device_name, device_manufacturer,
                                            device_model, sw_version, hw_version,
                                            topic.c_str(), nullptr,
                                            advanced_conf.c_str());

    String entity_discovery_topic = String(discovery_topic) + "/" + entity_type + "/" + uniqueId_prefix + "/" + entity_id + "/config";
    publish(mqtt_client, entity_discovery_topic, payload);
}

/**
 * @brief Формирование данных для публикации автодискавери топиков
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для публикации автодискавери информации
 * @param data показания
 * @param device_id
 * @param device_mac
 * @param entity свойства сенсора
 */
void publish_discovery_entity_extended(PubSubClient &mqtt_client, 
                                       const String &topic, 
                                       const String &discovery_topic,
                                       const SlaveData &data, 
                                       const String &device_id, 
                                       const String &device_mac,
                                       const char *const entity[MQTT_PARAM_COUNT])
{
    String device_name = get_device_name();
    String device_model = FPSTR(MODEL_NAMES[data.model]);
    String sw_version = String(F(FIRMWARE_VERSION)) + "." + data.version; // ESP_VERSION.ATTINY_VERSION
    String hw_version = F(HARDWARE_VERSION);                              // в дальнейшем можно модифицировать для гибкого определения версии hw
    String device_manufacturer = F(MANUFACTURER);
    
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, entity, 
                             device_name.c_str(), device_manufacturer.c_str(),
                             device_model.c_str(), sw_version.c_str(), hw_version.c_str());
}

/**
 * @brief Формирование данных для публикации автодискавери топиков
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для публикации автодискавери информации
 * @param device_id
 * @param device_mac
 * @param entity свойства сенсора
 * @param json_attributes_template аттрибуты сенсора
 * @param channel индекс канала при наличии
 * @param channel_name enum типа канала из интерфейса настройки
 */
void publish_discovery_entity_channel(PubSubClient &mqtt_client, 
                                      const String &topic, 
                                      const String &discovery_topic,
                                      const String &device_id, 
                                      const String &device_mac,
                                      const char *const entity[MQTT_PARAM_COUNT],
                                      const String &json_attributes_template,
                                      const int channel = NONE,
                                      const int channel_name = NONE)
{
    String entity_type = FPSTR(entity[0]);
    String entity_name = FPSTR(entity[1]);
    String entity_id = FPSTR(entity[2]);
    String state_class = FPSTR(entity[3]);
    String device_class = FPSTR(entity[4]);
    String unit_of_meas = FPSTR(entity[5]);
    String entity_category = FPSTR(entity[6]);
    String icon = FPSTR(entity[7]);
    String advanced_conf = FPSTR(entity[8]);

    String uniqueId_prefix = get_device_name();

    update_channel_names(channel, channel_name, entity_id, entity_name);

    LOG_INFO(F("MQTT: DISCOVERY:  Sensor: ") << entity_name);

    String payload = build_entity_discovery(topic.c_str(),
                                            entity_type.c_str(), entity_name.c_str(), entity_id.c_str(),
                                            state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                            entity_category.c_str(), icon.c_str(),
                                            device_id.c_str(), device_mac.c_str(),
                                            true, nullptr, nullptr,
                                            nullptr, nullptr, nullptr,
                                            topic.c_str(), json_attributes_template.c_str(),
                                            advanced_conf.c_str());

    String entity_discovery_topic = String(discovery_topic) + "/" + entity_type + "/" + uniqueId_prefix + "/" + entity_id + "/config";
    publish(mqtt_client, entity_discovery_topic, payload);
}

/**
 * @brief Публикация общих сенсоров устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для публикации автодискавери информации
 * @param data Данные из Attiny
 * @param device_id
 * @param device_mac
 */
void publish_discovery_general_entities(PubSubClient &mqtt_client, 
                                        const String &topic, 
                                        const String &discovery_topic, 
                                        const SlaveData &data, 
                                        const String &device_id, 
                                        const String &device_mac)
{
    publish_discovery_entity_extended(mqtt_client, topic, discovery_topic, data, device_id, device_mac, ENTITY_RESETS);
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_TIMESTAMP);
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_PERIOD_MIN);
    /* Сенсор с атрибутами  Группа №1 */
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_VOLTAGE);
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_VOLTAGE_DIFF);
    /* Сенсор с атрибутами  Группа №2 */
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_RSSI);
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_ROUTER_MAC);
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_MAC);
    publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_IP);
}

/**
 * @brief Добавление аттрибута сенсора в json
 *
 * @param json_attributes json с аттрибутами
 * @param entity свойства сенсора
 * @param channel номер канала 0 или 1
 * @param channel_name название канала (вода, электричество, газ, тепло)
 */
void add_entity_attribute(JsonObject &json_attributes, 
                          const char *const entity[MQTT_PARAM_COUNT], 
                          const int channel, 
                          const int channel_name)
{
    String entity_id = FPSTR(entity[2]);
    String entity_name = FPSTR(entity[1]);
    update_channel_names(channel, channel_name, entity_id, entity_name);
    json_attributes[entity_name] = String(F("{{ value_json.")) + entity_id + F(" | is_defined }}");
}

String channel_entity_attributes(const int channel, const int channel_name)
{
    String json_attributes_template;
    DynamicJsonDocument json_doc(JSON_DYNAMIC_MSG_BUFFER);
    JsonObject json_attributes = json_doc.to<JsonObject>();

    add_entity_attribute(json_attributes, ENTITY_CHANNEL_IMP, channel, channel_name);
    add_entity_attribute(json_attributes, ENTITY_CHANNEL_DELTA, channel, channel_name);
    add_entity_attribute(json_attributes, ENTITY_CHANNEL_ADC, channel, channel_name);
    add_entity_attribute(json_attributes, ENTITY_CHANNEL_SERIAL, channel, channel_name);
    add_entity_attribute(json_attributes, ENTITY_CHANNEL_FACTOR, channel, channel_name);
    add_entity_attribute(json_attributes, ENTITY_CHANNEL_CNAME, channel, channel_name);
    add_entity_attribute(json_attributes, ENTITY_CHANNEL_CTYPE, channel, channel_name);
    
    serializeJson(json_attributes, json_attributes_template);
    return json_attributes_template;
}

/**
 * @brief Публикация сведений устройства по каналам
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для discovery данных
 * @param device_id device_id
 * @param device_mac device_mac
 * @param channel_is_work канал используется (тип входа не NONE)
 * @param channel номер канала 0 или 1
 * @param channel_name название канала (вода, электричество, газ, тепло)
 */
void publish_discovery_channel_entities(PubSubClient &mqtt_client, 
                                        const String &topic, 
                                        const String &discovery_topic,
                                        const String &device_id, 
                                        const String &device_mac,
                                        const bool channel_is_work,
                                        const int channel,
                                        const int channel_name)
{
    // Публикуем настройку типа входа, даже если он отключён
    publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_CHANNEL_CTYPE, "", channel, channel_name);
    
    if (!channel_is_work)
    {
        LOG_INFO(F("MQTT: Channel ") + String(channel) + F(" is turned off"));
        return;
    }

    String json_attributes_template = channel_entity_attributes(channel, channel_name);

    switch (channel_name) 
    {
        case CounterName::WATER_COLD:
        case CounterName::WATER_HOT:
        case CounterName::PORTABLE_WATER:
        case CounterName::OTHER:
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_WATER_TOTAL, json_attributes_template, channel, channel_name);
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_WATER_TOTAL_CFG, "", channel, channel_name);
            break;
        case CounterName::GAS:
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_GAS_TOTAL, json_attributes_template, channel, channel_name);
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_GAS_TOTAL_CFG, "", channel, channel_name);
            break;
        case CounterName::ELECTRO:
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_ELECTRO_TOTAL, json_attributes_template, channel, channel_name);
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_ELECTRO_TOTAL_CFG, "", channel, channel_name);
            break;
        case CounterName::HEAT:
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_HEAT_TOTAL, json_attributes_template, channel, channel_name);
            publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_HEAT_TOTAL_CFG, "", channel, channel_name);
            break;
    }

    publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_CHANNEL_SERIAL, "", channel, channel_name);
    publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_CHANNEL_FACTOR, "", channel, channel_name);
    publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac, ENTITY_CHANNEL_CNAME, "", channel, channel_name);

}

/**
 * @brief Публикация автодисковери топиков для устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic Корневой топик
 * @param data Данные из Attiny
 * @param sett Настройки Ватериуса
 */
void publish_discovery(PubSubClient &mqtt_client, 
                       const String &topic, 
                       const String &discovery_topic, 
                       const SlaveData &data, 
                       const Settings &sett)
{
    LOG_INFO(F("MQTT: Publishing discovery topic"));
    unsigned long start_time = millis();

    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();

    LOG_INFO(F("MQTT: General entities"));
    publish_discovery_general_entities(mqtt_client, topic, discovery_topic, data, device_id, device_mac);

    LOG_INFO(F("MQTT: Channel entities"));
    publish_discovery_channel_entities(mqtt_client, topic, discovery_topic, device_id, device_mac, 
                                       channel_is_work(data.counter_type0), 0, sett.counter0_name);
    publish_discovery_channel_entities(mqtt_client, topic, discovery_topic, device_id, device_mac, 
                                       channel_is_work(data.counter_type1), 1, sett.counter1_name);

    LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start_time << F(" milliseconds elapsed"));
}
