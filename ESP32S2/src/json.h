/**
 * @file json.h
 * @brief Формирование JSON с показаниями
 * @version 0.1
 * @date 2023-01-20
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef JSON_h_
#define JSON_h_

#include <ArduinoJson.h>
#include "setup.h"
#include "voltage.h"
#include "master_i2c.h"

/**
    Конвертирует настройки и показания в json

    @param sett настройки.
    @param data показания.
    @param cdata расчитанные показатели.
    @param voltage мониторинг питания.
    @param json_data json документ в который будут записаны данные.
*/
extern void get_json_data(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data);

#endif
