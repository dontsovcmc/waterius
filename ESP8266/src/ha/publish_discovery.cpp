#include <ArduinoJson.h>
#include "publish_discovery.h"
#include "resources.h"
#include "utils.h"
#include "porting.h"
#include "Logging.h"
#include "utils.h"
#include "discovery_entity.h"
#include "publish.h"
#include "core/discovery.h"

/**
 * @brief Топик конфига сущности: <discovery>/<тип>/<устройство>/<имя>/config
 */
static String entity_config_topic(const String &discovery_topic, const String &entity_type, const String &entity_id)
{
    return discovery_topic + "/" + entity_type + "/" + get_device_name() + "/" + entity_id + "/config";
}

/**
 * @brief Публикация конфига сущности
 *
 * Всегда с retain, независимо от mqtt_retain: Home Assistant берёт конфиги
 * у брокера при своём старте, а спящее устройство переслать их не может.
 */
static bool publish_entity_config(PubSubClient &mqtt_client, const String &config_topic, const String &payload)
{
    return publish(mqtt_client, config_topic, payload, true);
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
 * @param device_name Имя устройства
 * @param device_manufacturer Имя производителя
 * @param device_model Модель устройства
 * @param sw_version Версия прошивки
 * @param hw_version Версия железа
 * @return true конфиг опубликован
 */
bool publish_discovery_entity(PubSubClient &mqtt_client, 
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

    return publish_entity_config(mqtt_client, entity_config_topic(discovery_topic, entity_type, entity_id), payload);
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
 * @return true конфиг опубликован
 */
bool publish_discovery_entity_extended(PubSubClient &mqtt_client, 
                                       const String &topic, 
                                       const String &discovery_topic,
                                       const AttinyData &data, 
                                       const String &device_id, 
                                       const String &device_mac,
                                       const char *const entity[MQTT_PARAM_COUNT])
{
    String device_name = get_device_name();
    String device_model = FPSTR(MODEL_NAMES[data.model]);
    String sw_version = String(F(FIRMWARE_VERSION)) + "." + data.version; // ESP_VERSION.ATTINY_VERSION
    String hw_version = F(HARDWARE_VERSION);                              // в дальнейшем можно модифицировать для гибкого определения версии hw
    String device_manufacturer = F(MANUFACTURER);
    
    return publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, entity,
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
 * @return true конфиг опубликован
 */
bool publish_discovery_entity_channel(PubSubClient &mqtt_client, 
                                      const String &topic, 
                                      const String &discovery_topic,
                                      const String &device_id, 
                                      const String &device_mac,
                                      const char *const entity[MQTT_PARAM_COUNT],
                                      const String &json_attributes_template,
                                      const int channel = HA_NONE,
                                      const int channel_name = HA_NONE)
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

    update_channel_names(channel, channel_name, entity_id, entity_name);

    LOG_INFO(F("MQTT: DISCOVERY:  Sensor: ") << entity_name);

    String payload = build_entity_discovery(topic.c_str(),
                                            entity_type.c_str(), entity_name.c_str(), entity_id.c_str(),
                                            state_class.c_str(), device_class.c_str(), unit_of_meas.c_str(),
                                            entity_category.c_str(), icon.c_str(),
                                            device_id.c_str(), device_mac.c_str(),
                                            true, nullptr, nullptr,
                                            nullptr, nullptr, nullptr,
                                            topic.c_str(), json_attributes_template.length() > 0 ? json_attributes_template.c_str() : nullptr,
                                            advanced_conf.c_str());

    return publish_entity_config(mqtt_client, entity_config_topic(discovery_topic, entity_type, entity_id), payload);
}

/**
 * @brief Удаление сущности канала из Home Assistant
 *
 * Пустой конфиг с retain - способ, которым спецификация HA удаляет сущность
 * и снимает её конфиг у брокера.
 *
 * @param mqtt_client клиент MQTT
 * @param discovery_topic топик автодискавери
 * @param entity свойства сенсора
 * @param channel номер канала
 * @param channel_name enum типа канала из интерфейса настройки
 * @return true пустой конфиг опубликован
 */
bool clear_discovery_entity_channel(PubSubClient &mqtt_client,
                                    const String &discovery_topic,
                                    const char *const entity[MQTT_PARAM_COUNT],
                                    const int channel,
                                    const int channel_name)
{
    String entity_type = FPSTR(entity[0]);
    String entity_name;
    String entity_id = FPSTR(entity[2]);
    update_channel_names(channel, channel_name, entity_id, entity_name);

    return clear_retained(mqtt_client, entity_config_topic(discovery_topic, entity_type, entity_id));
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
 * @return true опубликованы все конфиги
 */
bool publish_discovery_general_entities(PubSubClient &mqtt_client,
                                        const String &topic,
                                        const String &discovery_topic,
                                        const AttinyData &data,
                                        const String &device_id,
                                        const String &device_mac)
{
    // Сведения об устройстве (модель, версии) едут с одной сущностью, HA
    // склеивает остальные с ней по identifiers
    static const char *const *const ENTITIES[] = {
        ENTITY_TIMESTAMP, ENTITY_PERIOD_MIN, ENTITY_SEND_ON_CONSUMPTION, ENTITY_VACATION,
        ENTITY_ALARM_RESET, ENTITY_ACK_WATERIUS, ENTITY_ACK_HTTP, ENTITY_ACK_MQTT,
        ENTITY_VOLTAGE, ENTITY_VOLTAGE_DIFF,
        ENTITY_RSSI, ENTITY_ROUTER_MAC, ENTITY_MAC, ENTITY_IP, ENTITY_WIFI_CHANNEL,
        ENTITY_WIWI_CONNECT_ERRORS, ENTITY_NTP_ERRORS, ENTITY_OTA_ERROR, ENTITY_MODEL,
    };

    // На первом отказе останавливаемся: при зависшем сокете каждая следующая
    // публикация ждала бы таймаута, а Wi-Fi всё это время жёг батарейки
    if (!publish_discovery_entity_extended(mqtt_client, topic, discovery_topic, data, device_id, device_mac, ENTITY_RESETS))
    {
        return false;
    }
    for (const char *const *entity : ENTITIES)
    {
        if (!publish_discovery_entity(mqtt_client, topic, discovery_topic, device_id, device_mac, entity))
        {
            return false;
        }
    }
    return true;
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
    JsonDocument json_doc;
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

struct ChannelEntityRef
{
    const char *const *entity;
    ChannelEntity group;
};

/**
 * @brief Сущности показаний для ресурса: сенсор и поле для правки
 *
 * Имя у всех ресурсов одно (chN), разные только класс и единица.
 *
 * Для неизвестного ресурса обе ссылки - nullptr
 */
static void readings_entities(const int channel_name, const char *const *&total, const char *const *&total_cfg)
{
    switch (channel_name)
    {
    case CounterName::WATER_COLD:
    case CounterName::WATER_HOT:
    case CounterName::PORTABLE_WATER:
        total = ENTITY_WATER_TOTAL;
        total_cfg = ENTITY_WATER_TOTAL_CFG;
        return;
    case CounterName::OTHER:
        total = ENTITY_OTHER_TOTAL;
        total_cfg = ENTITY_OTHER_TOTAL_CFG;
        return;
    case CounterName::GAS:
        total = ENTITY_GAS_TOTAL;
        total_cfg = ENTITY_GAS_TOTAL_CFG;
        return;
    case CounterName::ELECTRO:
        total = ENTITY_ELECTRO_TOTAL;
        total_cfg = ENTITY_ELECTRO_TOTAL_CFG;
        return;
    case CounterName::HEAT_GCAL:
        total = ENTITY_HEAT_GCAL_TOTAL;
        total_cfg = ENTITY_HEAT_GCAL_TOTAL_CFG;
        return;
    case CounterName::HEAT_KWT:
        total = ENTITY_HEAT_KWT_TOTAL;
        total_cfg = ENTITY_HEAT_KWT_TOTAL_CFG;
        return;
    }
    total = nullptr;
    total_cfg = nullptr;
}

/**
 * @brief Публикация сведений устройства по каналам
 *
 * Сущность, которая входу такого типа не нужна, удаляется у брокера: после
 * смены типа входа она иначе осталась бы в Home Assistant навсегда.
 *
 * @param mqtt_client клиент MQTT
 * @param topic топик для публикации
 * @param discovery_topic топик для discovery данных
 * @param device_id device_id
 * @param device_mac device_mac
 * @param counter_type тип входа attiny (CounterType)
 * @param channel номер канала 0 или 1
 * @param channel_name название канала (вода, электричество, газ, тепло)
 * @return true опубликованы все конфиги
 */
bool publish_discovery_channel_entities(PubSubClient &mqtt_client,
                                        const String &topic,
                                        const String &discovery_topic,
                                        const String &device_id,
                                        const String &device_mac,
                                        const uint8_t counter_type,
                                        const int channel,
                                        const int channel_name)
{
    const char *const *total;
    const char *const *total_cfg;
    readings_entities(channel_name, total, total_cfg);

    const bool readings_wanted = channel_entity_wanted(counter_type, ChannelEntity::READINGS);
    const bool known_resource = total != nullptr;
    if (!known_resource)
    {
        // Публиковать нечего, но удалить старое можно: имя у всех ресурсов одно
        total = ENTITY_WATER_TOTAL;
        total_cfg = ENTITY_WATER_TOTAL_CFG;
    }

    const ChannelEntityRef entities[] = {
        {ENTITY_CHANNEL_CTYPE, ChannelEntity::INPUT_TYPE},
        {ENTITY_CHANNEL_ALARM_WET, ChannelEntity::WET},
        {total, ChannelEntity::READINGS},
        {total_cfg, ChannelEntity::READINGS},
        {ENTITY_CHANNEL_SERIAL, ChannelEntity::SERIAL_NUMBER},
        {ENTITY_CHANNEL_FACTOR, ChannelEntity::FACTOR},
        {ENTITY_CHANNEL_CNAME, ChannelEntity::RESOURCE},
        {ENTITY_CHANNEL_ALARM_VOL_CFG, ChannelEntity::ALARM_CONFIG},
        {ENTITY_CHANNEL_ALARM_RATE_CFG, ChannelEntity::ALARM_CONFIG},
        {ENTITY_CHANNEL_ALARM_HOURS_CFG, ChannelEntity::ALARM_CONFIG},
        {ENTITY_CHANNEL_ALARM_STOP_CFG, ChannelEntity::ALARM_CONFIG},
        {ENTITY_CHANNEL_ALARM_FLOW, ChannelEntity::ALARM_STATE},
        {ENTITY_CHANNEL_ALARM_LEAK, ChannelEntity::ALARM_STATE},
        {ENTITY_CHANNEL_ALARM_STOP, ChannelEntity::ALARM_STATE},
    };

    // Атрибуты - только у сенсора показаний
    const String attributes = readings_wanted && known_resource
                                  ? channel_entity_attributes(channel, channel_name)
                                  : String();

    for (const ChannelEntityRef &ref : entities)
    {
        bool ok;
        if (!channel_entity_wanted(counter_type, ref.group))
        {
            ok = clear_discovery_entity_channel(mqtt_client, discovery_topic, ref.entity, channel, channel_name);
        }
        else if (ref.group == ChannelEntity::READINGS && !known_resource)
        {
            continue;
        }
        else
        {
            ok = publish_discovery_entity_channel(mqtt_client, topic, discovery_topic, device_id, device_mac,
                                                  ref.entity, ref.entity == total ? attributes : String(),
                                                  channel, channel_name);
        }

        // Остановка на первом отказе - см. publish_discovery_general_entities
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Публикация автодисковери топиков для устройства
 *
 * @param mqtt_client клиент MQTT
 * @param topic Корневой топик
 * @param discovery_topic топик автодискавери
 * @param data Данные из Attiny
 * @param sett Настройки Ватериуса
 * @return true опубликованы все конфиги
 */
bool publish_discovery(PubSubClient &mqtt_client,
                       const String &topic,
                       const String &discovery_topic,
                       const AttinyData &data,
                       const Settings &sett)
{
    LOG_INFO(F("MQTT: Publishing discovery topic"));
    unsigned long start_time = millis();

    String device_id = String(getChipId());
    String device_mac = get_mac_address_hex();

    LOG_INFO(F("MQTT: General entities"));
    const bool ok = publish_discovery_general_entities(mqtt_client, topic, discovery_topic, data, device_id, device_mac) &&
                    publish_discovery_channel_entities(mqtt_client, topic, discovery_topic, device_id, device_mac,
                                                       data.counter_type0, 0, sett.counter0_name) &&
                    publish_discovery_channel_entities(mqtt_client, topic, discovery_topic, device_id, device_mac,
                                                       data.counter_type1, 1, sett.counter1_name);

    LOG_INFO(F("MQTT: Discovery topic published: ") << millis() - start_time << F(" milliseconds elapsed"));
    return ok;
}
