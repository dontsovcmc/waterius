/**
 * @file publish_data.h
 * @brief Функция публикации показаний в MQTT
 * @version 0.1
 * @date 2023-02-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef HA_PUBLISHDATA_H_
#define HA_PUBLISHDATA_H_

#include <PubSubClient.h>
#include <ArduinoJson.h>

extern void publish_data(PubSubClient &mqtt_client, String &topic, JsonDocument &json_data, bool auto_discovery);

#endif

