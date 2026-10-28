#include <gtest/gtest.h>
#include "core/power.h"

/*
Флаг «батарейки сели» и процент заряда считаются по одному среднему напряжению
сеанса. Набор проверок: порог своей модели, гистерезис Ватериуса-2 и шкала
процентов - концы, порог и монотонность.
*/

namespace
{
    const uint16_t USB = 5155;          // измерено на стенде
}

TEST(UsbPowered, ExternalSupplyIsToldByVoltage)
{
    EXPECT_TRUE(usb_powered(USB));
    EXPECT_TRUE(usb_powered(USB_VOLTAGE_MV + 1));
    EXPECT_FALSE(usb_powered(USB_VOLTAGE_MV));
}

TEST(LowVoltage, ThresholdOfThisModel)
{
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    EXPECT_EQ(BATTERY_LOW_MV, 3300);
#else
    EXPECT_EQ(BATTERY_LOW_MV, 2900);
#endif
    EXPECT_TRUE(low_voltage(BATTERY_LOW_MV - 1, false));
    EXPECT_FALSE(low_voltage(BATTERY_LOW_MV, false));
    EXPECT_FALSE(low_voltage(USB, false));
}

TEST(LowVoltage, FlagIsReleasedOnlyAboveHysteresis)
{
    const uint16_t release = BATTERY_LOW_MV + BATTERY_LOW_HYSTERESIS_MV;
    EXPECT_TRUE(low_voltage(release - 1, true));
    EXPECT_FALSE(low_voltage(release, true));
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    // Отскок на пару шагов АЦП после падения под порог флаг не снимает
    EXPECT_TRUE(low_voltage(BATTERY_LOW_MV + 36, true));
#endif
}

TEST(BatteryLevelW2, Ends)
{
    EXPECT_EQ(0, battery_level_w2(2500));
    EXPECT_EQ(0, battery_level_w2(3200));
    EXPECT_EQ(1, battery_level_w2(3213));
    EXPECT_EQ(50, battery_level_w2(3850));
    EXPECT_EQ(100, battery_level_w2(4500));
    EXPECT_EQ(100, battery_level_w2(USB));
}

TEST(BatteryLevelW2, LowFlagComesBeforeZero)
{
    EXPECT_EQ(7, battery_level_w2(W2_BATTERY_LOW_MV));
}

TEST(BatteryLevelW2, FallsAsVoltageFalls)
{
    uint8_t previous = 100;
    for (uint16_t mv = 4600; mv >= 2400; mv -= 5)
    {
        const uint8_t level = battery_level_w2(mv);
        EXPECT_LE(level, previous) << mv << " мВ";
        previous = level;
    }
}

TEST(BatteryLevel, ClassicHasNoPercent)
{
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    EXPECT_EQ(battery_level_w2(3600), battery_level(3600, false));
    EXPECT_EQ(battery_level_w2(3600), battery_level(3600, true));
#else
    EXPECT_EQ(100, battery_level(3100, false));
    EXPECT_EQ(0, battery_level(2850, true));
#endif
}
