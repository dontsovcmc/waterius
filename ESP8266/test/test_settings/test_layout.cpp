#include <gtest/gtest.h>
#include <stddef.h>
#include "core/types.h"

/*
Раскладка Settings в памяти.

Конфигурация лежит в EEPROM сырыми байтами: store_config делает EEPROM.put
структурой целиком, load_config — EEPROM.get. Ни имён полей, ни версии полей
там нет, только смещения. Поэтому вставка поля в середину структуры means,
что каждое прошитое устройство прочитает чужие байты: пароль от Wi-Fi
превратится в мусор, показания счётчика — в случайное число.

Защита от этого одна: новые поля берутся из хвостового резерва, а смещения
старых зафиксированы тестом. Числа ниже подсмотрены не из головы — это
раскладка, с которой устройства ходят в поле.
*/

TEST(SettingsLayout, SizeIsFixed)
{
    // Тот же размер проверяет static_assert в types.h — здесь он нужен,
    // чтобы падение было читаемым, а не стеной ошибок компилятора
    EXPECT_EQ(sizeof(Settings), 960u);
}

TEST(SettingsLayout, ExistingFieldsDidNotMove)
{
    EXPECT_EQ(offsetof(Settings, version), 0u);
    EXPECT_EQ(offsetof(Settings, wakeup_per_min), 612u);
    EXPECT_EQ(offsetof(Settings, period_min_tuned), 614u);
    EXPECT_EQ(offsetof(Settings, last_send), 616u);
    EXPECT_EQ(offsetof(Settings, base_time), 872u);
    EXPECT_EQ(offsetof(Settings, voltage_cal), 880u);
}

TEST(SettingsLayout, NewFieldsLiveInTheReservedTail)
{
    // Резерв начинался на 882 байте — всё, что добавляем, обязано лежать
    // за этой границей, иначе поехали бы старые поля
    EXPECT_GE(offsetof(Settings, last_time_sync), 882u);
    EXPECT_GE(offsetof(Settings, wakeups_since_sync), 882u);
    EXPECT_GE(offsetof(Settings, period_min_full), 882u);
    EXPECT_LT(offsetof(Settings, period_min_full), 960u);
}

TEST(SettingsLayout, RepurposedReservedByteStayedInPlace)
{
    // ntp_sync_count занял бывший reserved8. Смещение то же самое, значит
    // у прошитых устройств там ноль — ровно то, что нужно: прогрев пройдёт
    // заново, а не начнётся с мусора
    EXPECT_EQ(offsetof(Settings, ntp_sync_count), 881u);
}

TEST(SettingsLayout, TimeFieldsAreEightBytes)
{
    // Раскладка считалась при time_t == 8 байт. Четырёхбайтовый time_t
    // сдвинул бы всё, что лежит после base_time, и заодно вернул бы
    // проблему 2038 года
    Settings sett;

    EXPECT_EQ(sizeof(sett.base_time), 8u);
    EXPECT_EQ(sizeof(sett.last_send), 8u);
    EXPECT_EQ(sizeof(sett.last_time_sync), 8u);
}

TEST(SettingsDefaults, FreshDeviceHasNoSyncHistory)
{
    // Нулевое время не проходит is_valid_time, значит первое же пробуждение
    // пойдёт синхронизироваться. Так же выглядит конфигурация устройства,
    // прошитого старой версией: в резерве были нули.
    Settings sett;

    EXPECT_EQ(sett.last_time_sync, 0);
    EXPECT_EQ(sett.wakeups_since_sync, 0);
    EXPECT_EQ(sett.ntp_error_counter, 0);
    EXPECT_EQ(sett.ntp_sync_count, 0);
    EXPECT_EQ(sett.period_min_full, 0);   // поправка ещё не измерена
}
