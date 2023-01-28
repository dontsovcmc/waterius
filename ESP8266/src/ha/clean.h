/**
 * @file clean.h
 * @brief Содержит функции  
 * @version 0.1
 * @date 2023-01-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef HA_CLEAN_H_
#define HA_CLEAN_H_

#include <PubSubClient.h>

extern void clean_discovery(PubSubClient &mqtt_client, String &topic);

#endif