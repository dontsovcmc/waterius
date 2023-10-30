#ifndef CONFIG_H_
#define CONFIG_H_

#include <Arduino.h>
#include <WiFiClient.h>
#include "master_i2c.h"

#include "setup.h"

/* Сохраняем конфигурацию в EEPROM */
extern void store_config(const Settings &sett);

/* Читаем конфигурацию из EEPROM */
extern bool load_config(Settings &sett);

/* Инициализируем начальные значения конфигурации */
extern bool init_config(Settings &sett);

/* Обновляем данные в конфиге*/
extern void update_config(Settings &sett, const SlaveData &data, const CalculatedData &cdata);

/* Рассчитываем текущие показания */
extern void calculate_values(Settings &sett, const SlaveData &data, CalculatedData &cdata);

/* Очищаем память и инициализируем настройки по умолчанию */
extern void factory_reset(Settings &sett);

#endif