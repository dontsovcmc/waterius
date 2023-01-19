/**
 * @file json.h
 * @brief Формирование JSON с показаниями 
 * @version 0.1
 * @date 2023-01-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef _JSON_h
#define _JSON_h

#include <ArduinoJson.h>
#include "setup.h"
#include "master_i2c.h"
#include "voltage.h"

extern void get_json_data(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, Voltage &voltage, DynamicJsonDocument &json_data);

#endif
