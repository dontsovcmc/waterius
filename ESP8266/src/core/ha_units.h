#ifndef _WATERIUS_CORE_HA_UNITS_h
#define _WATERIUS_CORE_HA_UNITS_h

/*
Единицы измерения и классы устройств Home Assistant.

HA проверяет пару device_class + unit_of_measurement и молча отбрасывает
сущность, если пара недопустима. В логе HA появляется строка вида

  The unit of measurement `kWt` is not valid together with device class `energy`

а у пользователя просто нет сенсора — ни ошибки в прошивке, ни намёка в
интерфейсе. Именно так работал тип счётчика "тепло (кВт)": единица была
написана как "kWt", такой в HA нет (#356).

Строки живут здесь, чтобы у прошивки и у тестов был один источник: в
ha/resources.h они подставляются в PROGMEM-таблицу сущностей.

Часть чистого ядра src/core: без Arduino.h.
*/

#include "types.h"

#define HA_CLASS_ENERGY "energy"
#define HA_CLASS_WATER "water"
#define HA_CLASS_GAS "gas"
#define HA_CLASS_VOLTAGE "voltage"
#define HA_CLASS_BATTERY "battery"
#define HA_CLASS_SIGNAL "signal_strength"
#define HA_CLASS_TIMESTAMP "timestamp"

#define HA_UNIT_M3 "m³"
#define HA_UNIT_KWH "kWh"
#define HA_UNIT_GCAL "Gcal"
#define HA_UNIT_VOLT "V"
#define HA_UNIT_PERCENT "%"
#define HA_UNIT_DBM "dBm"

/*
Допустима ли пара для Home Assistant.

Список взят из DEVICE_CLASS_UNITS самого HA (проверено на 2025.11 и 2026.8).
Классы без единиц измерения (timestamp и сущности без device_class)
считаются допустимыми только с пустой единицей — именно так их и требует HA.
*/
bool ha_unit_matches_device_class(const char *device_class, const char *unit);

#endif
