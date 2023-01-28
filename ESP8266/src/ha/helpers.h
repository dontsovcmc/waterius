#ifndef HA_HELPERS_H_
#define HA_HELPERS_H_

#include <PubSubClient.h>
#include <ArduinoJson.h>

extern void publish_data_to_single_topic(PubSubClient &mqtt_client, String &topic, DynamicJsonDocument &json_data);
extern void publish_data_to_multiple_topics(PubSubClient &mqtt_client, String &topic, DynamicJsonDocument &json_data);
extern void publish(PubSubClient &mqtt_client, String &topic, String &payload);
extern void clean(PubSubClient &mqtt_client, String &topic);

#endif