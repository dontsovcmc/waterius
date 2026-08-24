#include <gtest/gtest.h>
#include "core/portal.h"

/*
Время жизни режима настройки (#305).

Таймер — подстраховка от «ушёл и оставил включённым», поэтому он обязан
однажды сработать. Но пока человек настраивает — переходит по страницам,
сохраняет формы — обрывать его нельзя: на заполнение полей и ожидание
импульса от счётчика десяти минут не хватает.

Ниже время в миллисекундах, как его отдаёт millis().
*/

static const uint32_t MINUTE = 60000UL;

TEST(Portal, FreshPortalIsAlive)
{
    EXPECT_FALSE(portal_expired(0, 0, 0));
    EXPECT_FALSE(portal_expired(9 * MINUTE, 0, 0));
}

TEST(Portal, IdlePortalExpires)
{
    // Никакой активности не было: окно отсчитывается от старта
    EXPECT_TRUE(portal_expired(PORTAL_IDLE_MS, 0, 0));
    EXPECT_TRUE(portal_expired(11 * MINUTE, 0, 0));
}

TEST(Portal, ActivityExtendsWindow)
{
    // Человек открыл страницу на девятой минуте — окно считается заново
    const uint32_t activity = 9 * MINUTE;

    EXPECT_FALSE(portal_expired(activity + 9 * MINUTE, 0, activity));
    EXPECT_TRUE(portal_expired(activity + PORTAL_IDLE_MS, 0, activity));
}

TEST(Portal, TotalTimeIsLimited)
{
    // Активность не отменяет общий предел: страница определения счётчика
    // опрашивает устройство сама, и забытая вкладка держала бы Wi-Fi
    const uint32_t activity = PORTAL_MAX_MS - MINUTE;

    EXPECT_TRUE(portal_expired(PORTAL_MAX_MS, 0, activity));
    EXPECT_TRUE(portal_time_limit_reached(PORTAL_MAX_MS, 0));
}

TEST(Portal, IdleAndTotalAreDistinguishable)
{
    // Две причины выхода означают разное, в логе они не должны сливаться
    EXPECT_TRUE(portal_expired(PORTAL_IDLE_MS, 0, 0));
    EXPECT_FALSE(portal_time_limit_reached(PORTAL_IDLE_MS, 0));
}

TEST(Portal, IdleWindowMatchesAttiny)
{
    /*
    Питание снимает attiny: после команды 'E' он держит ЕСП ровно
    SETUP_TIME_MSEC (Attiny85/src/Setup.h). Если окна разъедутся, продление
    станет бессмысленным — одна сторона выключится раньше другой.
    */
    EXPECT_EQ(PORTAL_IDLE_MS, 600000UL);
    EXPECT_GT(PORTAL_MAX_MS, PORTAL_IDLE_MS);
}

TEST(Portal, MillisOverflowDoesNotCutSetupShort)
{
    /*
    millis() переполняется через 49 суток. Момент сам по себе ничего не
    значит: если сравнивать моменты напрямую, портал после переполнения
    закрылся бы мгновенно.
    */
    const uint32_t started = 0xFFFFF000UL;   // до переполнения осталось ~4 с
    const uint32_t now = started + 5 * MINUTE; // счётчик уже перевалил через ноль

    EXPECT_LT(now, started);   // проверяем, что тест действительно про переполнение
    EXPECT_FALSE(portal_expired(now, started, started));
    EXPECT_TRUE(portal_expired(started + PORTAL_IDLE_MS, started, started));
}
