#include <gtest/gtest.h>
#include "core/alarm.h"

/*
Пересчёт порогов тревог и разбор ответа attiny (issue #202).

Пороги пользователь задаёт в литрах в час (в ваттах для электричества), а
attiny сравнивает тики сторожевого таймера по 250 мс. Пересчёт здесь, потому
что вес импульса и тип счётчика знает только ЕСП.
*/

TEST(Alarm, WaterThreshold)
{
    // 600 л/ч при 10 л/имп — импульс раз в минуту, это 240 тиков
    EXPECT_EQ(flow_to_interval_ticks(600, 10, false), 240);

    // Тот же расход у счётчика на 100 л/имп — импульс раз в 10 минут
    EXPECT_EQ(flow_to_interval_ticks(600, 100, false), 2400);
}

TEST(Alarm, ElectricityThreshold)
{
    // 3000 Вт при 1000 имп/кВт*ч — 3000 импульсов в час, это 4,8 тика
    EXPECT_EQ(flow_to_interval_ticks(3000, 1000, true), 4);
}

TEST(Alarm, DisabledThreshold)
{
    // Ноль — тревога выключена, и это умолчание у прошитых устройств
    EXPECT_EQ(flow_to_interval_ticks(0, 10, false), 0);
}

TEST(Alarm, UnknownFactorIsNotDivided)
{
    // Вес импульса ноль — делить не на что, тревога выключена
    EXPECT_EQ(flow_to_interval_ticks(600, 0, false), 0);
    EXPECT_EQ(flow_to_interval_ticks(3000, 0, true), 0);
}

TEST(Alarm, TinyThresholdSaturates)
{
    /*
    Порог 1 л/ч при 100 л/имп — импульс раз в 100 часов, в uint16 не влезает.
    Насыщаем, а не обнуляем: ноль означал бы "выключено", то есть обратное
    тому, что просил пользователь.
    */
    EXPECT_EQ(flow_to_interval_ticks(1, 100, false), UINT16_MAX);
}

TEST(Alarm, HugeThresholdStaysArmed)
{
    /*
    Порог выше, чем счётчик способен выдать: округление вниз дало бы ноль,
    то есть молча выключенную тревогу.
    */
    EXPECT_EQ(flow_to_interval_ticks(60000, 1, false), 1);
    EXPECT_GT(flow_to_interval_ticks(60000, 60000, true), 0);
}

TEST(Alarm, BitsPerInput)
{
    // Вход 0 — биты 1-3, вход 1 — биты 4-6, бит 0 занят флагом питания
    const uint8_t flags = ATTINY_FLAG_ESP_POWERED_LONG |
                          (ALARM_FLOW << ATTINY_ALARM_SHIFT0) |
                          (ALARM_WET << ATTINY_ALARM_SHIFT1);

    EXPECT_EQ(alarm_bits(flags, INPUT0_RED, ATTINY_VER_ALARM), ALARM_FLOW);
    EXPECT_EQ(alarm_bits(flags, INPUT1_BLUE, ATTINY_VER_ALARM), ALARM_WET);
}

TEST(Alarm, AllBitsOfOneInput)
{
    // Три тревоги на одном входе держатся одновременно
    const uint8_t flags = (ALARM_FLOW | ALARM_LEAK | ALARM_WET) << ATTINY_ALARM_SHIFT1;

    EXPECT_EQ(alarm_bits(flags, INPUT1_BLUE, ATTINY_VER_ALARM),
              ALARM_FLOW | ALARM_LEAK | ALARM_WET);
    EXPECT_EQ(alarm_bits(flags, INPUT0_RED, ATTINY_VER_ALARM), 0);
}

TEST(Alarm, OldAttinyHasNoAlarms)
{
    /*
    У attiny 40 в этом байте только флаг питания, остальные биты ничего не
    значат. Прочитать их как тревоги — показать пользователю аварию на
    исправном устройстве.
    */
    const uint8_t flags = 0xFF;

    EXPECT_EQ(alarm_bits(flags, INPUT0_RED, ATTINY_VER_POWER_FLAGS), 0);
    EXPECT_EQ(alarm_bits(flags, INPUT1_BLUE, ATTINY_VER_POWER_FLAGS), 0);
    EXPECT_NE(alarm_bits(flags, INPUT0_RED, ATTINY_VER_ALARM), 0);
}

TEST(Alarm, ConfigurableNeedsKnownFactor)
{
    // До первой настройки вес импульса — спецзначение, пересчитывать не из чего
    EXPECT_FALSE(alarm_configurable(CounterType::NAMUR, AUTO_IMPULSE_FACTOR));
    EXPECT_FALSE(alarm_configurable(CounterType::NAMUR, AS_COLD_CHANNEL));
    EXPECT_FALSE(alarm_configurable(CounterType::NAMUR, 0));

    EXPECT_TRUE(alarm_configurable(CounterType::NAMUR, 10));
}

TEST(Alarm, ConfigurableForElectricity)
{
    // Электричество разрешено: формула другая, но вес импульса известен
    EXPECT_TRUE(alarm_configurable(CounterType::ELECTRONIC, 1000));
}

TEST(Alarm, DisabledInputHasNoAlarms)
{
    EXPECT_FALSE(alarm_configurable(CounterType::NONE, 10));
}
