#ifndef _WATERIUS_CORE_POWER_h
#define _WATERIUS_CORE_POWER_h

#include <stdint.h>
#include "types.h"   // WATERIUS_MODEL_2

/*
Заряд батареек по среднему напряжению сеанса - тому же числу, что уходит как
voltage. Просадку не учитываем: на Ватериусе-2 она одинакова от USB и от
батареек, на Классике по ней поднимали тревогу две трети исправных устройств.

Часть чистого ядра src/core: без Arduino.h и логирования.
*/

// Выше этого питание внешнее. Нужно только OTA: свежие батарейки Ватериуса-2 дают столько же
#define USB_VOLTAGE_MV 4600

// Классик меряет питание после стабилизатора 3,0 В: ниже порога батарейки уже не держат его
#define CLASSIC_BATTERY_LOW_MV 2900

// Ватериус-2 меряет батарейки напрямую. Порог совпадает с OTA_MIN_VOLTAGE_MV
#define W2_BATTERY_LOW_MV 3300

// Шаг АЦП attiny около 18 мВ: без запаса флаг у порога переключался бы каждый сеанс
#define W2_BATTERY_LOW_HYSTERESIS_MV 50

// Ватериус-2: ниже ESP за стабилизатором не держит Wi-Fi, это 0 %. Выше - запас до свежих батареек
#define W2_BATTERY_EMPTY_MV 3200
#define W2_BATTERY_FULL_MV 4500

#if WATERIUS_MODEL == WATERIUS_MODEL_2
#define BATTERY_LOW_MV W2_BATTERY_LOW_MV
#define BATTERY_LOW_HYSTERESIS_MV W2_BATTERY_LOW_HYSTERESIS_MV
#else
#define BATTERY_LOW_MV CLASSIC_BATTERY_LOW_MV
#define BATTERY_LOW_HYSTERESIS_MV 0
#endif

bool usb_powered(const uint16_t avg_mv);

/*
Требуют ли батарейки замены.

@param avg_mv среднее напряжение за сеанс
@param was_low флаг прошлого сеанса: снимается он выше порога на гистерезис
*/
bool low_voltage(const uint16_t avg_mv, const bool was_low);

/*
Ватериус-2: запас до отказа Wi-Fi, 0-100 %, линейно от W2_BATTERY_EMPTY_MV
до W2_BATTERY_FULL_MV.
*/
uint8_t battery_level_w2(const uint16_t avg_mv);

/*
Уровень для посылки. Классик напряжение до конца держит ровным, процента у него
нет: 100 или 0 по флагу.

@param low флаг этого сеанса
*/
uint8_t battery_level(const uint16_t avg_mv, const bool low);

#endif
