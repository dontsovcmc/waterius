#ifndef _WATERIUS_CORE_DISCOVERY_h
#define _WATERIUS_CORE_DISCOVERY_h

/*
Автообнаружение Home Assistant: когда его публиковать и из чего оно состоит.

Устройство спит и не слышит `homeassistant/status`, поэтому переслать конфиги
по просьбе HA не может. Вместо этого оно помнит отпечаток опубликованного и
публикует заново, как только отпечаток разошёлся с текущим состоянием.

Часть чистого ядра src/core: без Arduino.h.
*/

#include <stdint.h>

// То, от чего зависит содержимое конфигов
struct DiscoveryState
{
    const char *firmware;       // FIRMWARE_VERSION: набор сущностей и sw_version
    uint8_t attiny_version;     // тоже в sw_version
    uint8_t model;
    uint8_t counter_type0;
    uint8_t counter_type1;
    uint8_t counter_name0;
    uint8_t counter_name1;
    const char *topic;          // stat_t и cmd_t сущностей
    const char *discovery_topic;
};

// Ноль не возвращает никогда: им в настройках помечено «ничего не опубликовано»
uint32_t discovery_signature(const DiscoveryState &state);

// Группы сущностей входа
enum class ChannelEntity : uint8_t
{
    INPUT_TYPE,     // ctype
    WET,            // alarm_wet
    READINGS,       // chN: sensor и number
    SERIAL_NUMBER,
    FACTOR,
    RESOURCE,       // cname
    ALARM_CONFIG,   // av, ar, ah, as
    ALARM_STATE,    // alarm_flow, alarm_leak, alarm_stop
};

/*
Нужна ли сущность входу такого типа. Ненужную прошивка не пропускает, а
удаляет у брокера: иначе после смены типа она осталась бы в HA навсегда.
*/
bool channel_entity_wanted(uint8_t counter_type, ChannelEntity entity);

#endif
