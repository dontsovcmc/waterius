#ifndef HA_HELPERS_H_
#define HA_HELPERS_H_

#include <PubSubClient.h>
#include <ArduinoJson.h>

extern void publish(PubSubClient &mqtt_client, String &topic, String &payload);
extern void publish_simple(PubSubClient &mqtt_client, String &topic, String &payload);
extern void publish_chunked(PubSubClient &mqtt_client, String &topic, String &payload);

#endif