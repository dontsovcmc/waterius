#include <ArduinoJson.h>
#include "publish_discovery.h"
#include "resources.h"
#include "utils.h"
#include "porting.h"
#include "Logging.h"
#include "discovery_entity.h"
#include "publish.h"

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
 * @param channel_name enum типа канала из интерфейса настройки
 */
void publish_discovery_entity(PubSubClient &mqtt_client, String &topic, String &discovery_topic,
                              const SlaveData &data, String &device_id, String &device_mac,
                              const char *const entities[][MQTT_PARAM_COUNT],
                              int entity_indx,
                              bool extended = false,
                              int attrs_index = NONE,
                              int attrs_count = NONE,
                              int channel = NONE,
                              int channel_name = NONE)
{

    String device_name = "";
    String device_model = "";
    String sw_version = "";
    String hw_version = "";
    String device_manufacturer = "";
    String entity_type = FPSTR(entities[entity_indx][0]);
    String entity_name = FPSTR(entities[entity_indx][1]);
    String entity_id = FPSTR(entities[entity_indx][2]);
    String state_class = FPSTR(entities[entity_indx][3]);
    String device_class = FPSTR(entities[entity_indx][4]);
    String unit_of_meas = FPSTR(entities[entity_indx][5]);
    String entity_category = FPSTR(entities[entity_indx][6]);
    String icon = FPSTR(entities[entity_indx][7]);
    String advanced_conf = FPSTR(entities[entity_indx][8]);
    String json_attributes_topic = "";
    String json_attributes_template = "";

    String uniqueId_prefix = get_device_name();

    update_channel_names(channel, channel_name, entity_id, entity_name);

    if (extended)
    {
        device_name = get_device_name();
        device_model = FPSTR(MODEL_NAMES[data.model]);
        sw_version = String(F(FIRMWARE_VERSION)) + "." + data.version; // ESP_VERSION.ATTINY_VERSION
        hw_version = F(HARDWARE_VERSION);                              // в дальнейшем можно модифицировать для гибкого определения версии hw
        device_manufacturer = F(MANUFACTURER);
    }

    LOG_INFO(F("MQTT: DISCOVERY:  Sensor: ") << entity_name);

    if ((attrs_index != NONE) && (attrs_count != NONE))
    {
        json_attributes_topic = topic;
        json_attributes_template = get_attributes_template(entities, attrs_index, attrs_count, channel, channel_name);
    }

    String payload = build_entity_discovery(topic.c_str(),
                                            entity_type.c_str(), entity_name.c_str(), entity_id.c_str(),
                                            state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                            entity_category.c_str(), icon.c_str(),
                                            device_id.c_str(), device_mac.c_str(),
                                            true, device_name.c_str(), device_manufacturer.c_str(),
                                            device_model.c_str(), sw_version.c_str(), hw_version.c_str(),
                                            json_attributes_topic.c_str(), json_attributes_template.c_str(),
                                            advanced_conf.c_str());

    String entity_discovery_topic = String(discovery_topic) + "/" + entity_type + "/" + uniqueId_prefix + "/" + entity_id + "/config";
    publish(mqtt_client, entity_discovery_topic, payload);
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
    // добавляем одиночные сенсоры из массива GENERAL_ENTITIES
    bool extended;
    for (int i = 0; i < GENERAL_ENTITIES_LEN; i++)
    {
        extended = i == 0; // в первый сенсор дописываем всю информацию про устройство
        publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, GENERAL_ENTITIES, i, extended);
    }
}

/**
 * @brief Публикация сведений устройства по каналам
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param data показания
 */
void publish_discovery_channel_entities(PubSubClient &mqtt_client, String &topic, String &discovery_topic, 
                                        const SlaveData &data, const Settings &sett,
                                        String &device_id, String &device_mac)
{
    // Сенсоры по каналам, холодная и горячая вода

    switch (sett.counter0_name) 
    {
        case CounterName::WATER_COLD:
        case CounterName::WATER_HOT:
        case CounterName::PORTABLE_WATER:
        case CounterName::OTHER:
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 0, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 1, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 2, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 3, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 4, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 5, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 6, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 7, false, 1, 8, 0, sett.counter0_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 8, false, 1, 8, 0, sett.counter0_name);
            break;
        case CounterName::GAS:
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_GAS_ENTITIES, 0, false, 1, 5, 0, sett.counter0_name);
            break;
        case CounterName::ELECTRO:
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_ELECTRO_ENTITIES, 0, false, 1, 5, 0, sett.counter0_name);
        case CounterName::HEAT:
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_HEAT_ENTITIES, 0, false, 1, 5, 0, sett.counter0_name);
            break;
    }

    switch (sett.counter1_name) 
    {
        case CounterName::WATER_COLD:
        case CounterName::WATER_HOT:
        case CounterName::PORTABLE_WATER:
        case CounterName::OTHER:
            //publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 0, false, 1, 5, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 0, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 1, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 2, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 3, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 4, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 5, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 6, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 7, false, 1, 8, 1, sett.counter1_name);
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_WATER_ENTITIES, 8, false, 1, 8, 1, sett.counter1_name);
            break;
        case CounterName::GAS:
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_GAS_ENTITIES, 0, false, 1, 5, 1, sett.counter1_name);
            break;
        case CounterName::ELECTRO:
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_ELECTRO_ENTITIES, 0, false, 1, 5, 1, sett.counter1_name);
            break;
        case CounterName::HEAT:
            publish_discovery_entity(mqtt_client, topic, discovery_topic, data, device_id, device_mac, CHANNEL_HEAT_ENTITIES, 0, false, 1, 5, 1, sett.counter1_name);
            break;
    }
}

/**
 * @brief Публикация автодисковери топиков для устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic Корневой топик
 * @param data Данные измерений
 */
void publish_discovery(PubSubClient &mqtt_client, String &topic, String &discovery_topic, const SlaveData &data, const Settings &sett)
{
    LOG_INFO(F("MQTT: Publishing discovery topic"));
    unsigned long start_time = millis();

    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();

    LOG_INFO(F("MQTT: General entities"));
    publish_discovery_general_entities(mqtt_client, topic, discovery_topic, data, device_id, device_mac);

    LOG_INFO(F("MQTT: Channel entities"));
    publish_discovery_channel_entities(mqtt_client, topic, discovery_topic, data, sett, device_id, device_mac);

    LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start_time << F(" milliseconds elapsed"));
}
