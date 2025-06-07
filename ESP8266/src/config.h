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

/* Корректируем период пробуждения только для автоматического режима */
uint16_t tune_wakeup(const time_t &now, const time_t &base_time, const uint16_t &wakeup_per_min);

/* Сбрасываем скорректированный период после изменения периода пользователем */
extern void reset_period_min_tuned(Settings &sett);

/* Обновляем данные в конфиге*/
extern void update_config(Settings &sett, const SlaveData &data, const CalculatedData &cdata);

/* Рассчитываем текущие показания */
extern void calculate_values(Settings &sett, const SlaveData &data, CalculatedData &cdata);

/* Очищаем память и инициализируем настройки по умолчанию */
extern void factory_reset(Settings &sett);

#endif