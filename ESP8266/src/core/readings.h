#ifndef _WATERIUS_CORE_READINGS_h
#define _WATERIUS_CORE_READINGS_h

/*
Расчёт показаний счётчиков из импульсов, накопленных attiny85.

Часть чистого ядра src/core: без Arduino.h и логирования. Логи пишет
адаптер calculate_values() в config.cpp по флагам из ReadingsStatus.
*/

#include "types.h"

// Если пришло импульсов меньше 3, то перед нами 10л/имп. Если больше, то 1л/имп.
#define IMPULS_LIMIT_1 3

/*
Нештатные ситуации расчёта. Адаптер сообщает о них в лог.
*/
struct ReadingsStatus
{
    // Счётчик импульсов attiny оказался меньше стартового: показания
    // пересчитаны от нового старта, иначе получились бы миллионы кубометров
    bool impulses0_start_reset = false;
    bool impulses1_start_reset = false;
};

/*
Пересчитывает показания и приросты по текущему числу импульсов.

Вода, газ, тепло: factor — литров на импульс, показания в кубометрах,
                  прирост в литрах.
Электричество:    factor — импульсов на кВт*ч, показания и прирост в кВт*ч.

Канал с factor == 0 не пересчитывается.
*/
ReadingsStatus calc_readings(Settings &sett, const AttinyData &data, CalculatedData &cdata);

/*
Вес импульса: разворачивает спец. значения (авто-определение и
"как холодный канал") в конкретное число литров на импульс.
*/
uint16_t get_auto_factor(const uint32_t runtime_impulses,
                         const uint32_t impulses,
                         const uint16_t factor,
                         const uint16_t factor_cold);

/*
Переносит в снимок данных типы входов из живой копии.

Данные от attiny живут в двух экземплярах: снимок при загрузке (по нему
считаются показания и дельты) и живая копия, которая обновляется во время
сеанса. Команда из MQTT меняет тип входа только в живой копии, поэтому
payload, собранный из снимка, отдавал наверх старое значение, и селектор
в Home Assistant отщёлкивал обратно (#360).

Копируются только типы входов: импульсы обязаны остаться от момента
загрузки, на них уже посчитаны и отправлены дельты.
*/
void apply_counter_types(AttinyData &snapshot, const AttinyData &current);

#endif
