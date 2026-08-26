#ifndef _WATERIUS_CORE_ALARM_h
#define _WATERIUS_CORE_ALARM_h

/*
Тревоги: пересчёт порогов для attiny и разбор её ответа (issue #202).

Детекция живёт в attiny - только она видит импульсы, пока ЕСП спит. Но у
attiny нет ни веса импульса, ни типа счётчика, а деление на ней дорого,
поэтому пороги в понятных человеку единицах пересчитывает ЕСП, а attiny
получает готовое число тиков и только сравнивает.

Часть чистого ядра src/core: без Arduino.h.

Что означает каждая тревога и когда снимается - docs/alarms.md.
*/

#include "types.h"

/*
Тиков сторожевого таймера attiny (250мс) в часе.
*/
#define ALARM_TICKS_PER_HOUR 14400UL

/*
Порог расхода в тиках между импульсами.

  threshold - л/ч для объёма, Вт для электричества
  factor    - вес импульса: литров на импульс, у электричества наоборот,
              импульсов на кВт*ч (см. core/readings.cpp)

Объём:   импульсов/ч = threshold / factor          -> тиков = 14400 * factor / threshold
Электро: импульсов/ч = threshold * factor / 1000   -> тиков = 14400000 / (threshold * factor)

@return 0 если тревога выключена или посчитать нельзя
*/
uint16_t flow_to_interval_ticks(const uint16_t threshold, const uint16_t factor,
                                const bool electricity);

/*
Тревоги одного входа из байта флагов attiny.

@param input INPUT0_RED или INPUT1_BLUE
@return маска ALARM_*, ноль для attiny старее ATTINY_VER_ALARM: там в этом
        байте лежит только флаг питания
*/
uint8_t alarm_bits(const uint8_t attiny_flags, const uint8_t input,
                   const uint8_t attiny_version);

/*
Можно ли настроить тревоги на этом входе.

Электричество можно: формула другая, но вес импульса известен. Нельзя, пока
вес импульса не определён - до первой настройки там стоит спецзначение
(AUTO_IMPULSE_FACTOR или AS_COLD_CHANNEL), и пересчитывать не из чего.
*/
bool alarm_configurable(const uint8_t counter_type, const uint16_t factor);

#endif
