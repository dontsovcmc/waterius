#include "diagnostics.h"
#include "input.h"

namespace
{
    /*
    Импульсы, накопленные с момента ввода показаний. Счётчик attiny может
    оказаться меньше стартового - после сброса attiny или замены платы;
    считаем, что расхода не было, лишь бы не получить миллионы кубометров
    на разнице беззнаковых.
    */
    uint32_t consumed_impulses(const uint32_t impulses, const uint32_t impulses_start)
    {
        return (impulses > impulses_start) ? impulses - impulses_start : 0;
    }

    uint32_t consumed_liters(const uint32_t impulses, const uint32_t impulses_start,
                             const uint16_t factor)
    {
        return consumed_impulses(impulses, impulses_start) * factor;
    }

    /*
    Вес импульса канала ровно вдесятеро тяжелее соседского, и расход
    разошёлся во столько же раз.

    Пара весов обязательна: при одинаковых весах ошибка автоопределения в
    отношении расходов не видна вообще, а разлёт означает настоящую разницу
    потребления - плашка была бы враньём. Заодно пара называет виновного:
    подозреваем тот канал, у которого вес тяжелее.

    Делим, а не умножаем: знаменатель уже не меньше COMPARE_MIN_LITERS, а
    произведение на мусорных импульсах переполнило бы uint32_t.
    */
    bool factor_too_big(const uint16_t factor, const uint16_t factor_other,
                        const uint32_t liters, const uint32_t liters_other)
    {
        if (factor != factor_other * 10)
            return false;

        if (liters < COMPARE_MIN_LITERS || liters_other < COMPARE_MIN_LITERS)
            return false;

        return liters / liters_other >= SUSPICIOUS_RATIO;
    }

    /*
    Вход настроен, сосед за то же время считал, а этот не насчитал ничего.
    Так выглядит перепутанный тип входа, оборванный провод и счётчик, к
    которому забыли подключиться.

    Отключённый вход (тип NONE) и вход, до настройки которого не дошли,
    молчат законно.
    */
    bool silent_input(const uint8_t counter_type, const uint16_t factor,
                      const uint32_t impulses, const uint32_t impulses_start,
                      const uint32_t neighbour_impulses)
    {
        if (counter_type == CounterType::NONE || !factor_configured(factor))
            return false;

        if (consumed_impulses(impulses, impulses_start) > 0)
            return false;

        return neighbour_impulses >= NEIGHBOUR_MIN_IMPULSES;
    }
}

SetupProblems check_setup(const Settings &sett, const AttinyData &data)
{
    SetupProblems problems;

    const uint32_t impulses0 = consumed_impulses(data.impulses0, sett.impulses0_start);
    const uint32_t impulses1 = consumed_impulses(data.impulses1, sett.impulses1_start);

    problems.silent_input0 = silent_input(data.counter_type0, sett.factor0,
                                          data.impulses0, sett.impulses0_start, impulses1);
    problems.silent_input1 = silent_input(data.counter_type1, sett.factor1,
                                          data.impulses1, sett.impulses1_start, impulses0);

    /*
    Расходы сравнимы только между водой: у электричества factor - это
    импульсы на кВт*ч, то есть число из другой системы координат.
    */
    if (!is_water_counter(sett.counter0_name) || !is_water_counter(sett.counter1_name))
        return problems;

    if (!factor_configured(sett.factor0) || !factor_configured(sett.factor1))
        return problems;

    const uint32_t liters0 = consumed_liters(data.impulses0, sett.impulses0_start, sett.factor0);
    const uint32_t liters1 = consumed_liters(data.impulses1, sett.impulses1_start, sett.factor1);

    problems.factor_too_big0 = factor_too_big(sett.factor0, sett.factor1, liters0, liters1);
    problems.factor_too_big1 = factor_too_big(sett.factor1, sett.factor0, liters1, liters0);

    return problems;
}
