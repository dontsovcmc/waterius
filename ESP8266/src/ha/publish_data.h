#ifndef HA_PUBLISHDATA_H_
#define HA_PUBLISHDATA_H_

#include <PubSubClient.h>
#include <ArduinoJson.h>

extern void publish_data(PubSubClient &mqtt_client, String &topic, DynamicJsonDocument &json_data, bool auto_discovery);

#endif

