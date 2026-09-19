#include <gtest/gtest.h>
#include "core/power.h"

/*
Заряд батареек меряется просадкой под передачей, и у этой мерки есть область
применимости: она про батарейки. Устройство на внешнем питании просаживает
напряжение по своим причинам - кабель, источник, - и тревога о замене
несуществующих батареек там бессмысленна.

Отсюда состав проверок: граница между питаниями, поведение по обе стороны от
неё и целость шкалы процентов. Последнее - не придирка: обе ветки формулы
сшиты на 50 мВ, и разрыв шва означает уровень, растущий по мере разряда.
*/

namespace
{
    // Напряжения, встречающиеся в жизни
    const uint16_t USB = 5155;          // измерено на стенде
    const uint16_t FRESH = 4500;        // три щелочных элемента
    const uint16_t TIRED = 3600;
    const uint16_t DEAD = 2800;         // ниже абсолютного порога

    // Просадки
    const uint16_t CALM = 10;
    const uint16_t STAND_SAG = 118;     // измерено на стенде: кабель, не батарейки
    const uint16_t WORN = 100;
}

TEST(UsbPowered, ExternalSupplyIsToldByVoltage)
{
    EXPECT_TRUE(usb_powered(USB));
    EXPECT_TRUE(usb_powered(USB_VOLTAGE_MV + 1));
    EXPECT_FALSE(usb_powered(USB_VOLTAGE_MV));
    EXPECT_FALSE(usb_powered(FRESH));
}

TEST(LowVoltage, SagOnExternalSupplyIsNotEmptyBatteries)
{
    EXPECT_FALSE(low_voltage(USB, STAND_SAG));
    EXPECT_EQ(100, battery_level(USB, STAND_SAG));
}

TEST(LowVoltage, SagOnBatteriesMeansReplacement)
{
    EXPECT_TRUE(low_voltage(FRESH, WORN));
    EXPECT_EQ(0, battery_level(FRESH, WORN));
}

TEST(LowVoltage, AbsoluteThresholdDecidesOnItsOwn)
{
    EXPECT_TRUE(low_voltage(DEAD, CALM));
    EXPECT_EQ(0, battery_level(DEAD, CALM));
}

TEST(BatteryLevel, FreshBatteriesAreFull)
{
    EXPECT_FALSE(low_voltage(FRESH, CALM));
    EXPECT_EQ(100, battery_level(FRESH, 0));
}

TEST(BatteryLevel, ScaleIsContinuousWhereBranchesMeet)
{
    // Обе ветки формулы обязаны сойтись на LOW_BATTERY_DIFF_MV, иначе уровень
    // прыгнет: в этом и состоит цена смены порога в отрыве от коэффициента
    const uint8_t before = battery_level(TIRED, LOW_BATTERY_DIFF_MV);
    const uint8_t after = battery_level(TIRED, LOW_BATTERY_DIFF_MV + 1);
    EXPECT_EQ(25, before);
    EXPECT_LE(after, before);
    EXPECT_GE(after, before - 2);
}

TEST(BatteryLevel, FallsAsSagGrows)
{
    uint8_t previous = 101;
    for (uint16_t sag = 0; sag < ALERT_POWER_DIFF_MV; sag++)
    {
        const uint8_t level = battery_level(TIRED, sag);
        EXPECT_LE(level, previous) << "просадка " << sag << " мВ";
        EXPECT_LE(level, 100);
        previous = level;
    }
}
