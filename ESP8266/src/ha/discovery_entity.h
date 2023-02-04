#ifndef HA_DISCOVERY_ENTITY_H_
#define HA_DISCOVERY_ENTITY_H_

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "resources.h"

extern String build_entity_discovery(const char *mqtt_topic,
                                   const char *sensor_type,
                                   const char *sensor_name,
                                   const char *sensor_id,
                                   const char *state_class,
                                   const char *device_class,
                                   const char *unit_of_meas,
                                   const char *entity_category,
                                   const char *icon,
                                   const char *device_id,
                                   const char *device_mac,
                                   bool enabled_by_default = true,
                                   const char *device_name = "",
                                   const char *device_manufacturer = "",
                                   const char *device_model = "",
                                   const char *sw_version = "",
                                   const char *hw_version = "",
                                   const char *json_attributes_topic = "",
                                   const char *json_attributes_template = "");

extern void update_channel_names(int channel, String &entity_id, String &entity_name);

extern String get_attributes_template(const char *const attrs[][MQTT_PARAM_COUNT], int index, int count, int channel);

#endif
