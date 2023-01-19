#ifndef _HOMEASSISTANT_h
#define _HOMEASSISTANT_h

#include "master_i2c.h"
#include <PubSubClient.h>

extern void publish_discovery(PubSubClient &client, String &topic, const SlaveData &data, bool single_topic);

#endif