/**
 * @file home_assistant.h
 * @brief содерждит функции для формирования топиков MQTT для автоматического добавления в HomeAssistant
 * @version 0.1
 * @date 2023-01-18
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef _HOMEASSISTANT_h
#define _HOMEASSISTANT_h

#include "master_i2c.h"
#include <PubSubClient.h>

extern void publish_discovery(PubSubClient &client, String &topic, const SlaveData &data, bool single_topic);

#endif