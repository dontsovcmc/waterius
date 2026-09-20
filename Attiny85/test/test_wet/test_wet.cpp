#include <gtest/gtest.h>
#include "wet.h"

/*
Пороги датчика протечки (issue #202).

Вода - это сопротивление в десятки и сотни килоом, а не замыкание контактов,
поэтому уровень входа снимается АЦП. Ниже проверяется, что выбранные пороги
дают одно и то же решение на всём разбросе внутренней подтяжки attiny
(20..50кОм по даташиту) и на обеих платах: у Classic последовательно 3к6
(3к3 в линии + 300 Ом в общем проводе), у Ватериуса 2 - 1кОм.

Сопротивления в килоомах.
*/

static const double PULLUP_MIN = 20.0;   // даташит attiny85
static const double PULLUP_TYP = 30.0;   // измеренная, см. counter.h
static const double PULLUP_MAX = 50.0;   // даташит attiny85

static const double SERIES_CLASSIC = 3.6;
static const double SERIES_W2 = 1.0;

static const double PULLUPS[] = {PULLUP_MIN, PULLUP_TYP, PULLUP_MAX};
static const double BOARDS[] = {SERIES_CLASSIC, SERIES_W2};

// Разомкнутая линия: подтяжка тянет пин к Vcc, ток утечки пина отнимает
// единицы отсчётов.
static const uint16_t ADC_OPEN = 1023;
static const uint16_t ADC_OPEN_LEAKY = 1013;

/*
Код АЦП делителя: от Vcc через подтяжку к пину, от пина через резистор платы
и датчик на землю. Опорное - Vcc, 10 бит.
*/
static uint16_t adc_code(double r_sensor, double r_pullup, double r_series)
{
    const double low = r_sensor + r_series;
    const double code = 1024.0 * low / (low + r_pullup);
    return code > 1023.0 ? 1023 : (uint16_t)code;
}

/*
Модель делителя сверяется с таблицей замеров из counter.h: по каждой паре
"сопротивление - код" считаем, какая подтяжка её даёт, и она обязана лечь в
даташитные 20..50кОм. Иначе моделью нельзя обосновывать пороги.
*/
TEST(AdcCode, CalibrationTableImpliesDatasheetPullup)
{
    struct Measured { double sensor; uint16_t adc; double series; };
    const Measured table[] = {
        {0.0, 104, SERIES_CLASSIC},   // counter.h, Classic
        {1.5, 130, SERIES_CLASSIC},
        {5.5, 230, SERIES_CLASSIC},
        {0.0, 42, SERIES_W2},         // counter.h, Ватериус 2
        {1.5, 78, SERIES_W2},
        {5.5, 170, SERIES_W2},
    };

    for (const Measured &m : table)
    {
        const double pullup = (1024.0 - m.adc) / m.adc * (m.sensor + m.series);
        EXPECT_GE(pullup, PULLUP_MIN) << "R=" << m.sensor << "к, adc=" << m.adc;
        EXPECT_LE(pullup, PULLUP_MAX) << "R=" << m.sensor << "к, adc=" << m.adc;
    }
}

/*
"Замыкание": мокро - любая проводимость. Нижней границы нет, замыкание тоже
вода; верхнюю ставит сухое состояние - разомкнутая линия.
*/
TEST(Wet, AnyConductionIsWet)
{
    const double sensors[] = {0.0, 1.0, 10.0, 100.0, 200.0};

    for (double pullup : PULLUPS)
        for (double series : BOARDS)
            for (double sensor : sensors)
            {
                const uint16_t adc = adc_code(sensor, pullup, series);
                EXPECT_TRUE(adc_below(adc, LIMIT_WET))
                    << "R=" << sensor << "к, подтяжка " << pullup
                    << "к, плата " << series << "к, adc=" << adc;
            }
}

TEST(Wet, HundredKilohmIsWetOnAnyChip)
{
    // Требование задачи: 100кОм обязаны ловиться при любой подтяжке.
    EXPECT_EQ(adc_code(100.0, PULLUP_MIN, SERIES_CLASSIC), 858);
    EXPECT_EQ(adc_code(100.0, PULLUP_MAX, SERIES_CLASSIC), 690);
    EXPECT_LT(adc_code(100.0, PULLUP_MIN, SERIES_CLASSIC) + 50, LIMIT_WET);
}

TEST(Wet, OnlyOpenLineIsDry)
{
    EXPECT_FALSE(adc_below(ADC_OPEN, LIMIT_WET));
    EXPECT_FALSE(adc_below(ADC_OPEN_LEAKY, LIMIT_WET));
}

/*
"Размыкание": отдельное соглашение. В норме шлейф замкнут перемычкой или
резистором 5к6, тревога - сопротивление выше. Обрыв отдельно не ловим: он
даёт 1023 и попадает в ту же тревогу.
*/
TEST(WetNc, IntactLoopIsNormal)
{
    const double loops[] = {0.0, 5.6};

    for (double pullup : PULLUPS)
        for (double series : BOARDS)
            for (double loop : loops)
            {
                const uint16_t adc = adc_code(loop, pullup, series);
                EXPECT_TRUE(adc_below(adc, LIMIT_WET_NC))
                    << "шлейф " << loop << "к, подтяжка " << pullup
                    << "к, плата " << series << "к, adc=" << adc;
            }
}

TEST(WetNc, WorstCaseLoopKeepsMargin)
{
    // Худший случай исправного шлейфа - 5к6 при самой слабой подтяжке.
    const uint16_t worst = adc_code(5.6, PULLUP_MIN, SERIES_CLASSIC);
    EXPECT_EQ(worst, 322);
    EXPECT_GT(LIMIT_WET_NC - worst, 100);
}

TEST(WetNc, HighResistanceAndBreakAreAlarm)
{
    for (double pullup : PULLUPS)
        for (double series : BOARDS)
        {
            const uint16_t adc = adc_code(100.0, pullup, series);
            EXPECT_FALSE(adc_below(adc, LIMIT_WET_NC))
                << "подтяжка " << pullup << "к, плата " << series
                << "к, adc=" << adc;
        }

    EXPECT_FALSE(adc_below(ADC_OPEN, LIMIT_WET_NC));
}

/*
Пороги не пересекаются и оба ниже разомкнутой линии: иначе у "замыкания"
мокрым стал бы неподключённый вход, а у "размыкания" тревога не отличалась бы
от нормы.
*/
TEST(Limits, AreOrdered)
{
    EXPECT_LT(LIMIT_WET_NC, LIMIT_WET);
    EXPECT_LT(LIMIT_WET, ADC_OPEN_LEAKY);
}
