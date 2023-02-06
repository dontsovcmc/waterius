/**
 * @file subscribe.h
 * @brief Модуль с функциями подключени и подписки на изменения MQTT 
 * @version 0.1
 * @date 2023-02-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef HA_SUBSCRIBE_H_
#define HA_SUBSCRIBE_H_

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "setup.h"

extern void mqtt_callback(Settings &sett,DynamicJsonDocument &json_data, PubSubClient &mqtt_client, String &mqtt_topic, char *raw_topic, byte *raw_payload, unsigned int length);
extern bool mqtt_connect(Settings &sett, PubSubClient &mqtt_client);
extern bool mqtt_subscribe(PubSubClient &mqtt_client, String &mqtt_topic);
extern bool mqtt_unsubscribe(PubSubClient &mqtt_client, String &mqtt_topic);


#endif