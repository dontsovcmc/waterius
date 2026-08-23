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

/* Корректируем период пробуждения только для автоматического режима — core/wakeup.h */

/* Сбрасываем скорректированный период после изменения периода пользователем */
extern void reset_period_min_tuned(Settings &sett);

/* Обновляем данные в конфиге*/
/*
time_synced — получено ли время от NTP на этом пробуждении. Поправку частоты
attiny можно считать только по такому времени: между синхронизациями часы
идут по оценке, и разница "сколько проспали" в ней равна заказанному периоду
по построению.
*/
extern void update_config(Settings &sett, const AttinyData &data, const CalculatedData &cdata,
                          const bool time_synced);

/* Рассчитываем текущие показания */
extern void calculate_values(Settings &sett, const AttinyData &data, CalculatedData &cdata);

/* Очищаем память и инициализируем настройки по умолчанию */
extern void factory_reset(Settings &sett);

#endif