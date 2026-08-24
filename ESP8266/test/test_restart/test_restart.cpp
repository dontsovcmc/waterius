#include <gtest/gtest.h>
#include "core/restart.h"
#include "core/types.h"

/*
Тесты признака "ЕСП перезагрузилась при живом питании" (#354).

Два независимых источника: причина ресета у самой ЕСП и флаг attiny
"питание подано давно". Ложное срабатывание стоит дороже пропуска: плашка
про перезагрузку у исправного устройства пугает пользователя, а пропущенная
перезагрузка оставляет всё как было до задачи.
*/

namespace
{
    const uint8_t NEW_ATTINY = ATTINY_VER_POWER_FLAGS;      // 40
    const uint8_t OLD_ATTINY = ATTINY_VER_POWER_FLAGS - 1;  // 39
    const uint8_t POWERED_LONG = ATTINY_FLAG_ESP_POWERED_LONG;
    const uint8_t NO_FLAGS = 0;
}

// --- обычная жизнь устройства ---

TEST(EspRestarted, PowerOnIsNotRestart)
{
    // attiny подала питание, ЕСП загрузилась первый раз
    EXPECT_FALSE(esp_restarted(RST_DEFAULT, NEW_ATTINY, NO_FLAGS));
    EXPECT_FALSE(esp_restarted(RST_EXT_SYS, NEW_ATTINY, NO_FLAGS));
}

TEST(EspRestarted, SoftRestartIsNotRestart)
{
    // Наш собственный ESP.restart() в конце настройки: питание ещё живо,
    // но флаг attiny в этот момент не взведён - прошло меньше 5 секунд
    EXPECT_FALSE(esp_restarted(RST_SOFT_RESTART, NEW_ATTINY, NO_FLAGS));
}

TEST(EspRestarted, DeepSleepWakeIsNotRestart)
{
    EXPECT_FALSE(esp_restarted(RST_DEEP_SLEEP, NEW_ATTINY, NO_FLAGS));
}

// --- падения самой ЕСП: причина ресета ---

TEST(EspRestarted, WatchdogAndExceptionAreRestart)
{
    EXPECT_TRUE(esp_restarted(RST_WDT, NEW_ATTINY, NO_FLAGS));
    EXPECT_TRUE(esp_restarted(RST_EXCEPTION, NEW_ATTINY, NO_FLAGS));
    EXPECT_TRUE(esp_restarted(RST_SOFT_WDT, NEW_ATTINY, NO_FLAGS));
}

TEST(EspRestarted, CrashDetectedWithAnyAttiny)
{
    // Причина ресета - целиком дело ЕСП, attiny для неё не нужна.
    // Прошивка attiny обновляется только программатором, поэтому у
    // устройств в поле это единственный работающий признак
    EXPECT_TRUE(esp_restarted(RST_EXCEPTION, OLD_ATTINY, NO_FLAGS));
    EXPECT_TRUE(esp_restarted(RST_WDT, 0, NO_FLAGS));
}

// --- признак от attiny ---

TEST(EspRestarted, AttinyFlagIsRestart)
{
    // Питание подано давно, а ЕСП спрашивает данные как после загрузки:
    // так выглядит просадка питания или внешний сброс
    EXPECT_TRUE(esp_restarted(RST_DEFAULT, NEW_ATTINY, POWERED_LONG));
    EXPECT_TRUE(esp_restarted(RST_EXT_SYS, NEW_ATTINY, POWERED_LONG));
}

TEST(EspRestarted, NewerAttinyStillReports)
{
    EXPECT_TRUE(esp_restarted(RST_DEFAULT, NEW_ATTINY + 5, POWERED_LONG));
}

TEST(EspRestarted, OldAttinyByteIgnored)
{
    // У старой прошивки на этом месте резерв: значение случайно совпало
    // с флагом - верить ему нельзя
    EXPECT_FALSE(esp_restarted(RST_DEFAULT, OLD_ATTINY, POWERED_LONG));
    EXPECT_FALSE(esp_restarted(RST_DEFAULT, OLD_ATTINY, 0xFF));
}

TEST(EspRestarted, OtherFlagBitsDoNotTrigger)
{
    // Биты 1-7 оставлены про запас: чужой флаг не должен звать плашку
    EXPECT_FALSE(esp_restarted(RST_DEFAULT, NEW_ATTINY, 0x02));
    EXPECT_FALSE(esp_restarted(RST_DEFAULT, NEW_ATTINY, 0xFE));
    EXPECT_TRUE(esp_restarted(RST_DEFAULT, NEW_ATTINY, 0xFF));
}
