#ifndef CONFIG_H_
#define CONFIG_H_

#include <Arduino.h>
#include <WiFiClient.h>
#include "master_i2c.h"

#include "setup.h"

struct eepromCRC{
    uint16_t crc;
    uint16_t size;
};

/* Сохраняем конфигурацию в EEPROM */
extern void store_config(const Settings &sett);

/* Читаем конфигурацию из EEPROM */
extern bool load_config(Settings &sett);

/* Обновляем данные в конфиге*/
extern void update_config(Settings &sett, const SlaveData &data, const CalculatedData &cdata);

/* Рассчитываем текущие показания */
extern void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata);

#endif