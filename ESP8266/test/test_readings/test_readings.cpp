#include <gtest/gtest.h>
#include "core/readings.h"

/*
Тесты фиксируют текущее поведение расчёта показаний.
Ожидания описывают то, как прошивка считает сегодня, а не то, как хотелось бы.
Места, где поведение спорное, помечены комментарием со ссылкой на issue.
*/

namespace
{
    // Настройки одного холодного водяного счётчика на канале 0.
    // Второй канал выключен (factor1 = 0), чтобы не мешал.
    Settings water_settings(uint16_t factor = 10)
    {
        Settings sett;
        sett.counter0_name = CounterName::WATER_COLD;
        sett.factor0 = factor;
        sett.channel0_start = 0.0;
        sett.impulses0_start = 0;
        sett.impulses0_previous = 0;
        sett.factor1 = 0;
        return sett;
    }

    AttinyData impulses(uint32_t ch0, uint32_t ch1 = 0)
    {
        AttinyData data = {};
        data.impulses0 = ch0;
        data.impulses1 = ch1;
        return data;
    }
}

// --- вода: factor = литров на импульс, показания в кубометрах ---

TEST(Readings, WaterFactorConvertsLitresToCubicMeters)
{
    Settings sett = water_settings(10);   // 10 литров на импульс
    CalculatedData cdata;

    calc_readings(sett, impulses(300), cdata);

    EXPECT_FLOAT_EQ(cdata.channel0, 3.0);   // 300 имп * 10 л = 3000 л = 3 м3
}

TEST(Readings, WaterDeltaStaysInLitres)
{
    Settings sett = water_settings(10);
    sett.impulses0_previous = 200;
    CalculatedData cdata;

    calc_readings(sett, impulses(300), cdata);

    // Показания в кубометрах, а прирост — в литрах. Асимметрия намеренная:
    // сервер ждёт delta в литрах.
    EXPECT_FLOAT_EQ(cdata.channel0, 3.0);
    EXPECT_EQ(cdata.delta0, 1000u);
}

TEST(Readings, WaterOneLitrePerImpulse)
{
    Settings sett = water_settings(1);
    CalculatedData cdata;

    calc_readings(sett, impulses(1234), cdata);

    EXPECT_FLOAT_EQ(cdata.channel0, 1.234);
}

TEST(Readings, FractionalStartKeepsPrecision)
{
    Settings sett = water_settings(10);
    sett.channel0_start = 123.45;
    CalculatedData cdata;

    calc_readings(sett, impulses(100), cdata);

    EXPECT_NEAR(cdata.channel0, 124.45, 0.0001);
}

// --- электричество: factor = импульсов на кВт*ч ---

TEST(Readings, ElectroFactorIsImpulsesPerKwh)
{
    Settings sett = water_settings(1000);
    sett.counter0_name = CounterName::ELECTRO;
    CalculatedData cdata;

    calc_readings(sett, impulses(500), cdata);

    EXPECT_FLOAT_EQ(cdata.channel0, 0.5);   // 500 имп / 1000 имп на кВт*ч
}

TEST(Readings, ElectroDeltaLosesFractionalKwh)
{
    Settings sett = water_settings(1000);
    sett.counter0_name = CounterName::ELECTRO;
    CalculatedData cdata;

    calc_readings(sett, impulses(1500), cdata);

    EXPECT_FLOAT_EQ(cdata.channel0, 1.5);
    // delta объявлена uint32_t, поэтому дробная часть кВт*ч отбрасывается:
    // 1.5 кВт*ч уезжает на сервер как 1. Характеризация, не одобрение.
    EXPECT_EQ(cdata.delta0, 1u);
}

// --- граничные случаи ---

TEST(Readings, ZeroFactorLeavesChannelUntouched)
{
    Settings sett = water_settings(0);      // канал не настроен
    CalculatedData cdata;
    cdata.channel0 = 42.0;
    cdata.delta0 = 7;

    calc_readings(sett, impulses(300), cdata);

    EXPECT_FLOAT_EQ(cdata.channel0, 42.0);
    EXPECT_EQ(cdata.delta0, 7u);
}

TEST(Readings, CounterRollbackResetsStart)
{
    // attiny обнулился (замена батареек, сброс): импульсов пришло меньше,
    // чем было на старте. Без сброса стартового значения показания
    // улетели бы в миллионы кубометров.
    Settings sett = water_settings(10);
    sett.impulses0_start = 1000;
    sett.channel0_start = 55.0;
    CalculatedData cdata;

    ReadingsStatus status = calc_readings(sett, impulses(100), cdata);

    EXPECT_TRUE(status.impulses0_start_reset);
    EXPECT_EQ(sett.impulses0_start, 100u);
    EXPECT_FLOAT_EQ(cdata.channel0, 55.0);  // показания равны стартовым
}

TEST(Readings, NoRollbackNoStatusFlag)
{
    Settings sett = water_settings(10);
    sett.impulses0_start = 100;
    CalculatedData cdata;

    ReadingsStatus status = calc_readings(sett, impulses(300), cdata);

    EXPECT_FALSE(status.impulses0_start_reset);
    EXPECT_FALSE(status.impulses1_start_reset);
    EXPECT_EQ(sett.impulses0_start, 100u);
}

TEST(Readings, FirstCycleDeltaIsWholeVolume)
{
    // impulses0_previous == 0 — первый выход на связь после настройки.
    // Весь накопленный объём уезжает одной дельтой.
    Settings sett = water_settings(10);
    CalculatedData cdata;

    calc_readings(sett, impulses(300), cdata);

    EXPECT_EQ(cdata.delta0, 3000u);
}

TEST(Readings, NearUint32MaxImpulses)
{
    Settings sett = water_settings(10);
    sett.impulses0_start = 0xFFFFFFFF - 100;
    sett.impulses0_previous = 0xFFFFFFFF - 100;
    CalculatedData cdata;

    calc_readings(sett, impulses(0xFFFFFFFF), cdata);

    EXPECT_FLOAT_EQ(cdata.channel0, 1.0);   // 100 имп * 10 л = 1 м3
    EXPECT_EQ(cdata.delta0, 1000u);
}

TEST(Readings, ChannelsAreIndependent)
{
    // #319: каналы не должны влиять друг на друга и не должны меняться местами
    Settings sett;
    sett.counter0_name = CounterName::WATER_HOT;
    sett.counter1_name = CounterName::WATER_COLD;
    sett.factor0 = 10;
    sett.factor1 = 1;
    sett.channel0_start = 0.0;
    sett.channel1_start = 0.0;
    sett.impulses0_start = 0;
    sett.impulses1_start = 0;
    sett.impulses0_previous = 0;
    sett.impulses1_previous = 0;
    CalculatedData cdata;

    calc_readings(sett, impulses(100, 100), cdata);

    EXPECT_FLOAT_EQ(cdata.channel0, 1.0);   // 100 имп * 10 л
    EXPECT_FLOAT_EQ(cdata.channel1, 0.1);   // 100 имп * 1 л
}

TEST(Readings, DisabledSecondChannelIsNotCalculated)
{
    Settings sett = water_settings(10);     // factor1 == 0
    CalculatedData cdata;

    calc_readings(sett, impulses(100, 999999), cdata);

    EXPECT_FLOAT_EQ(cdata.channel1, 0.0);
    EXPECT_EQ(cdata.delta1, 0u);
}

// --- вес импульса: авто-определение ---

TEST(AutoFactor, SmallConsumptionMeansTenLitres)
{
    // за время настройки натикало <= 3 импульсов => счётчик 10 л/имп
    EXPECT_EQ(get_auto_factor(103, 100, AUTO_IMPULSE_FACTOR, 1), 10);
}

TEST(AutoFactor, LargeConsumptionMeansOneLitre)
{
    EXPECT_EQ(get_auto_factor(104, 100, AUTO_IMPULSE_FACTOR, 1), 1);
}

TEST(AutoFactor, NoImpulsesMeansTenLitres)
{
    EXPECT_EQ(get_auto_factor(100, 100, AUTO_IMPULSE_FACTOR, 1), 10);
}

TEST(AutoFactor, AsColdChannelCopiesColdFactor)
{
    EXPECT_EQ(get_auto_factor(500, 100, AS_COLD_CHANNEL, 10), 10);
}

TEST(AutoFactor, ExplicitFactorPassesThrough)
{
    EXPECT_EQ(get_auto_factor(500, 100, 100, 1), 100);
}

TEST(AutoFactor, MagicValuesShadowRealFactors)
{
    // Спец. значения 3 (авто) и 7 (как холодный) занимают те же числа, что и
    // настоящий вес импульса 3 и 7 л/имп: задать такой счётчик вручную
    // невозможно — значение будет истолковано как спец. режим.
    EXPECT_EQ(AUTO_IMPULSE_FACTOR, 3);
    EXPECT_EQ(AS_COLD_CHANNEL, 7);
    EXPECT_NE(get_auto_factor(500, 100, 3, 1), 3);
    EXPECT_NE(get_auto_factor(500, 100, 7, 1), 7);
}

// --- снимок данных и изменение типов входов по MQTT (#360) ---

TEST(CounterTypes, ChangedTypesReachTheSnapshot)
{
    // Команда из MQTT меняет тип входа: значение уходит в attiny и в живую
    // копию runtime_data. Снимок data, из которого формируется payload,
    // об этом не знал — HA получал обратно старый тип и селектор
    // отщёлкивал назад.
    AttinyData snapshot;
    snapshot.counter_type0 = CounterType::NAMUR;
    snapshot.counter_type1 = CounterType::NONE;

    AttinyData current;
    current.counter_type0 = CounterType::DISCRETE;
    current.counter_type1 = CounterType::ELECTRONIC;

    apply_counter_types(snapshot, current);

    EXPECT_EQ(snapshot.counter_type0, CounterType::DISCRETE);
    EXPECT_EQ(snapshot.counter_type1, CounterType::ELECTRONIC);
}

TEST(CounterTypes, BothChannelsAtOnce)
{
    // В issue симптом описан именно так: "меняется сразу оба типа входа"
    AttinyData snapshot;
    snapshot.counter_type0 = CounterType::NONE;
    snapshot.counter_type1 = CounterType::NONE;

    AttinyData current;
    current.counter_type0 = CounterType::NAMUR;
    current.counter_type1 = CounterType::NAMUR;

    apply_counter_types(snapshot, current);

    EXPECT_EQ(snapshot.counter_type0, CounterType::NAMUR);
    EXPECT_EQ(snapshot.counter_type1, CounterType::NAMUR);
}

TEST(CounterTypes, ImpulsesStayFromBoot)
{
    // Показания трогать нельзя: runtime_data перечитывается из attiny во
    // время сеанса, и импульсы там уже другие. На снимке посчитаны дельты,
    // которые уже отправлены — подмена значений сдвинула бы расход.
    AttinyData snapshot;
    snapshot.impulses0 = 100;
    snapshot.impulses1 = 200;
    snapshot.voltage = 3300;
    snapshot.version = 38;

    AttinyData current;
    current.impulses0 = 105;
    current.impulses1 = 207;
    current.voltage = 2900;
    current.version = 99;

    apply_counter_types(snapshot, current);

    EXPECT_EQ(snapshot.impulses0, 100u);
    EXPECT_EQ(snapshot.impulses1, 200u);
    EXPECT_EQ(snapshot.voltage, 3300);
    EXPECT_EQ(snapshot.version, 38);
}

TEST(CounterTypes, DisablingChannelReachesTheSnapshot)
{
    // Отключение входа — тоже смена типа, NONE == 255
    AttinyData snapshot;
    snapshot.counter_type1 = CounterType::NAMUR;

    AttinyData current;
    current.counter_type1 = CounterType::NONE;

    apply_counter_types(snapshot, current);

    EXPECT_EQ(snapshot.counter_type1, CounterType::NONE);
}
