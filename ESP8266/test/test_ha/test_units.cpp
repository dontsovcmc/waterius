#include <gtest/gtest.h>
#include "core/ha_units.h"

/*
Пары device_class + unit_of_measurement, которые прошивка отправляет в
Home Assistant.

HA проверяет такую пару и молча выбрасывает сущность, если она недопустима:
в лог HA падает строка, а пользователь видит просто отсутствие сенсора.
Так и жил тип "тепло (кВт)" — единица была написана "kWt" (#356).

Проверено на живом HA (2025.11 и 2026.8, docker): из 61 сущности, которые
шлёт прошивка, отвергалась ровно одна — та самая.
*/

// --- то, что прошивка реально шлёт (ha/resources.h) ---

TEST(HaUnits, EveryEntityOfTheFirmwareIsValid)
{
    struct Entity { const char *name; const char *device_class; const char *unit; };

    const Entity entities[] = {
        {"вода",            HA_CLASS_WATER,   HA_UNIT_M3},
        {"газ",             HA_CLASS_GAS,     HA_UNIT_M3},
        {"электричество",   HA_CLASS_ENERGY,  HA_UNIT_KWH},
        {"тепло Гкал",      HA_CLASS_ENERGY,  HA_UNIT_GCAL},
        {"тепло кВт",       HA_CLASS_ENERGY,  HA_UNIT_KWH},
        {"напряжение",      HA_CLASS_VOLTAGE, HA_UNIT_VOLT},
        {"батарея",         HA_CLASS_BATTERY, HA_UNIT_PERCENT},
        {"уровень сигнала", HA_CLASS_SIGNAL,  HA_UNIT_DBM},
    };

    for (const Entity &e : entities)
    {
        EXPECT_TRUE(ha_unit_matches_device_class(e.device_class, e.unit))
            << e.name << ": " << e.device_class << " + " << e.unit;
    }
}

TEST(HaUnits, TimestampHasNoUnit)
{
    EXPECT_TRUE(ha_unit_matches_device_class(HA_CLASS_TIMESTAMP, ""));
    EXPECT_FALSE(ha_unit_matches_device_class(HA_CLASS_TIMESTAMP, "s"));
}

TEST(HaUnits, EntityWithoutDeviceClassAcceptsAnything)
{
    // Импульсы, дельта, ADC, серийник — без класса устройства,
    // HA такие единицы не проверяет
    EXPECT_TRUE(ha_unit_matches_device_class("", "имп"));
    EXPECT_TRUE(ha_unit_matches_device_class(nullptr, "что угодно"));
}

// --- то, из-за чего issue ---

TEST(HaUnits, WattIsNotEnergy)
{
    // "kWt" не существует в HA: киловатт пишется kW и это мощность,
    // киловатт-час — kWh. Для device_class energy годится только второе.
    EXPECT_FALSE(ha_unit_matches_device_class(HA_CLASS_ENERGY, "kWt"));
    EXPECT_FALSE(ha_unit_matches_device_class(HA_CLASS_ENERGY, "kW"));
    EXPECT_TRUE(ha_unit_matches_device_class(HA_CLASS_ENERGY, "kWh"));
}

TEST(HaUnits, GcalIsValidEnergy)
{
    // Гкал допустим — проверено на HA 2025.11 и 2026.8. То есть тип
    // "тепло (Гкал)" работал и работает, ломался только "тепло (кВт)"
    EXPECT_TRUE(ha_unit_matches_device_class(HA_CLASS_ENERGY, "Gcal"));
}

TEST(HaUnits, CubicMetersAreNotEnergy)
{
    // Перепутать класс не менее легко, чем единицу
    EXPECT_FALSE(ha_unit_matches_device_class(HA_CLASS_ENERGY, HA_UNIT_M3));
    EXPECT_FALSE(ha_unit_matches_device_class(HA_CLASS_WATER, HA_UNIT_KWH));
}

TEST(HaUnits, EmptyUnitWithMeasuredClassIsRejected)
{
    // Сенсор воды без единицы измерения HA тоже не примет
    EXPECT_FALSE(ha_unit_matches_device_class(HA_CLASS_WATER, ""));
    EXPECT_FALSE(ha_unit_matches_device_class(HA_CLASS_ENERGY, nullptr));
}

TEST(HaUnits, UnknownDeviceClassNeedsEmptyUnit)
{
    // Класса нет в таблице — не выдумываем, считаем допустимым только
    // отсутствие единицы. Лучше падающий тест, чем пропущенная сущность.
    EXPECT_TRUE(ha_unit_matches_device_class("moisture", ""));
    EXPECT_FALSE(ha_unit_matches_device_class("moisture", "%"));
}
