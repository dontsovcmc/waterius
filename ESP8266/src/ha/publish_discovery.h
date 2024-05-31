/**
 * @file publish_discovery.h
 * @brief содерждит функции для формирования топиков MQTT для автоматического добавления в HomeAssistant
 * https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
 * 
 * @version 0.1
 * @date 2023-01-18
 *
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef HA_PUBLISH_DISCOVERY_H_
#define HA_PUBLISH_DISCOVERY_H_

#include "master_i2c.h"
#include <PubSubClient.h>

class Settings;

extern void publish_discovery(PubSubClient &mqtt_client, 
                              const String &topic, 
                              const String &discovery_topic, 
                              const SlaveData &data,
                              const Settings &sett);

#endif