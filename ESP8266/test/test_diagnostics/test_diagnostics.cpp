#include <gtest/gtest.h>
#include "core/diagnostics.h"

/*
Тесты проверок настройки счётчиков (#283).

Ловим две ошибки: вес импульса, завышенный автоопределением в 10 раз, и
вход, который настроили, но он ничего не считает.

Ложное срабатывание стоит дороже пропуска: плашка у исправного устройства
заставит человека перенастроить то, что работало. Поэтому большая часть
тестов - про то, когда проверка обязана молчать.
*/

namespace
{
    /*
    Два водяных счётчика, показания вводились при нулевых импульсах.
    */
    Settings two_water_counters(const uint16_t factor0, const uint16_t factor1)
    {
        Settings sett;
        sett.counter0_name = CounterName::WATER_HOT;
        sett.counter1_name = CounterName::WATER_COLD;
        sett.factor0 = factor0;
        sett.factor1 = factor1;
        sett.impulses0_start = 0;
        sett.impulses1_start = 0;
        return sett;
    }

    AttinyData counting(const uint32_t impulses0, const uint32_t impulses1)
    {
        AttinyData data = {};
        data.counter_type0 = CounterType::NAMUR;
        data.counter_type1 = CounterType::NAMUR;
        data.impulses0 = impulses0;
        data.impulses1 = impulses1;
        return data;
    }
}

// --- вес импульса завышен в 10 раз ---

TEST(SetupProblems, HeavyFactorDetected)
{
    // 300 импульсов по 10 л = 3000 л против 500 импульсов по 1 л = 500 л,
    // разлёт шестикратный при десятикратной разнице веса
    Settings sett = two_water_counters(10, 1);
    SetupProblems problems = check_setup(sett, counting(300, 500));

    EXPECT_TRUE(problems.factor_too_big0);
    EXPECT_FALSE(problems.factor_too_big1);
}

TEST(SetupProblems, HeavyFactorOnBlueInput)
{
    Settings sett = two_water_counters(1, 10);
    SetupProblems problems = check_setup(sett, counting(500, 300));

    EXPECT_FALSE(problems.factor_too_big0);
    EXPECT_TRUE(problems.factor_too_big1);
}

TEST(SetupProblems, Factor100Against10)
{
    // Пара 100 и 10 - та же ошибка на счётчиках с крупным импульсом
    Settings sett = two_water_counters(100, 10);
    SetupProblems problems = check_setup(sett, counting(30, 50));

    EXPECT_TRUE(problems.factor_too_big0);
}

TEST(SetupProblems, SameFactorNeverBlamed)
{
    // Веса одинаковые: разлёт расходов настоящий, а общая ошибка
    // автоопределения в отношении вообще не видна
    Settings sett = two_water_counters(10, 10);
    SetupProblems problems = check_setup(sett, counting(3000, 30));

    EXPECT_FALSE(problems.factor_too_big0);
    EXPECT_FALSE(problems.factor_too_big1);
}

TEST(SetupProblems, NonDecimalFactorPairIgnored)
{
    // 10 и 3 - не десятикратная пара, откуда бы она ни взялась
    Settings sett = two_water_counters(10, 3);
    SetupProblems problems = check_setup(sett, counting(3000, 3000));

    EXPECT_FALSE(problems.factor_too_big0);
    EXPECT_FALSE(problems.factor_too_big1);
}

TEST(SetupProblems, RatioBelowThreshold)
{
    // 3000 л против 800 л: вчетверо - обычная квартира, горячей воды
    // всегда уходит меньше
    Settings sett = two_water_counters(10, 1);
    SetupProblems problems = check_setup(sett, counting(300, 800));

    EXPECT_FALSE(problems.factor_too_big0);
}

TEST(SetupProblems, TooLittleWater)
{
    // На синем всего 199 л - сравнивать рано
    Settings sett = two_water_counters(10, 1);
    SetupProblems problems = check_setup(sett, counting(300, 199));

    EXPECT_FALSE(problems.factor_too_big0);
}

TEST(SetupProblems, NoConsumptionNoProblems)
{
    Settings sett = two_water_counters(10, 1);
    SetupProblems problems = check_setup(sett, counting(0, 0));

    EXPECT_FALSE(problems.factor_too_big0);
    EXPECT_FALSE(problems.factor_too_big1);
    EXPECT_FALSE(problems.silent_input0);
    EXPECT_FALSE(problems.silent_input1);
}

TEST(SetupProblems, NotWaterIgnored)
{
    // У электричества factor - импульсы на кВт*ч, сравнивать не с чем
    Settings sett = two_water_counters(10, 1);
    sett.counter0_name = CounterName::ELECTRO;
    SetupProblems problems = check_setup(sett, counting(300, 500));

    EXPECT_FALSE(problems.factor_too_big0);
    EXPECT_FALSE(problems.factor_too_big1);
}

TEST(SetupProblems, UnconfiguredFactorIgnored)
{
    // Заводские значения: вход ещё не настраивали
    Settings sett = two_water_counters(AS_COLD_CHANNEL, AUTO_IMPULSE_FACTOR);
    SetupProblems problems = check_setup(sett, counting(300, 500));

    EXPECT_FALSE(problems.factor_too_big0);
    EXPECT_FALSE(problems.factor_too_big1);
}

TEST(SetupProblems, ImpulsesRolledBack)
{
    // Attiny сбросилась: импульсов меньше стартовых. Разница беззнаковых
    // дала бы миллиарды литров и плашку на ровном месте
    Settings sett = two_water_counters(10, 1);
    sett.impulses0_start = 1000;
    sett.impulses1_start = 1000;
    SetupProblems problems = check_setup(sett, counting(10, 2000));

    EXPECT_FALSE(problems.factor_too_big0);
    EXPECT_FALSE(problems.factor_too_big1);
}

// --- вход не считает ---

TEST(SetupProblems, SilentInputDetected)
{
    Settings sett = two_water_counters(10, 1);
    SetupProblems problems = check_setup(sett, counting(0, NEIGHBOUR_MIN_IMPULSES));

    EXPECT_TRUE(problems.silent_input0);
    EXPECT_FALSE(problems.silent_input1);
}

TEST(SetupProblems, BothSilentIsFreshSetup)
{
    // Молчат оба - устройство просто недавно настроили
    Settings sett = two_water_counters(10, 1);
    SetupProblems problems = check_setup(sett, counting(0, 0));

    EXPECT_FALSE(problems.silent_input0);
    EXPECT_FALSE(problems.silent_input1);
}

TEST(SetupProblems, SilentNeighbourTooQuiet)
{
    Settings sett = two_water_counters(10, 1);
    SetupProblems problems = check_setup(sett, counting(0, NEIGHBOUR_MIN_IMPULSES - 1));

    EXPECT_FALSE(problems.silent_input0);
}

TEST(SetupProblems, DisabledInputIsNotSilent)
{
    Settings sett = two_water_counters(10, 1);
    AttinyData data = counting(0, 500);
    data.counter_type0 = CounterType::NONE;
    SetupProblems problems = check_setup(sett, data);

    EXPECT_FALSE(problems.silent_input0);
}

TEST(SetupProblems, UnconfiguredInputIsNotSilent)
{
    // Тип входа выбран, а до показаний и веса импульса не дошли
    Settings sett = two_water_counters(AS_COLD_CHANNEL, 1);
    SetupProblems problems = check_setup(sett, counting(0, 500));

    EXPECT_FALSE(problems.silent_input0);
}

TEST(SetupProblems, SilentInputWorksForAnyCounter)
{
    // Молчание входа не про воду: проверка годится и для электричества
    Settings sett = two_water_counters(10, 1000);
    sett.counter1_name = CounterName::ELECTRO;
    SetupProblems problems = check_setup(sett, counting(0, 500));

    EXPECT_TRUE(problems.silent_input0);
}
