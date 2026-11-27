/**
 * @file publish.h
 * @brief Функции публикации MQTT
 * @version 0.1
 * @date 2023-02-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef HA_PUBLISH_H_
#define HA_PUBLISH_H_

#include <PubSubClient.h>

extern bool publish(PubSubClient &mqtt_client, const String &topic, const String &payload, bool retain);
extern bool clear_retained(PubSubClient &mqtt_client, const String &topic);

#endif