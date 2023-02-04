/**
 * @brief 
 * 
 * @param raw_topic 
 * @param raw_payload 
 * @param length 
 */

#ifndef HA_SUBSCRIBE_H_
#define HA_SUBSCRIBE_H_

#include <Arduino.h>
#include <PubSubClient.h>
#include "setup.h"

extern void mqtt_callback(Settings &sett, char *raw_topic, byte *raw_payload, unsigned int length);
extern void subscribe_to_topic(PubSubClient &mqtt_client, String &mqtt_topic);

#endif