#include <gtest/gtest.h>
#include "core/portal_watchdog.h"

/*
Время жизни режима настройки (#305).

Таймер — подстраховка от «ушёл и оставил включённым», поэтому он обязан
однажды сработать. Но пока человек настраивает — переходит по страницам,
сохраняет формы — обрывать его нельзя: на заполнение полей и ожидание
импульса от счётчика десяти минут не хватает.

Таймер один: общего потолка на всю настройку нет. Забытая вкладка портал не
держит, потому что автоматические опросы окно не продлевают, а потолок
обрывал бы того, кто настраивает долго и всерьёз.

Ниже время в миллисекундах, как его отдаёт millis().
*/

static const uint32_t MINUTE = 60000UL;

TEST(PortalWatchdog, FreshPortalIsAlive)
{
    EXPECT_FALSE(portal_watchdog_fired(0, 0));
    EXPECT_FALSE(portal_watchdog_fired(9 * MINUTE, 0));
}

TEST(PortalWatchdog, IdlePortalExpires)
{
    EXPECT_TRUE(portal_watchdog_fired(PORTAL_WATCHDOG_MS, 0));
    EXPECT_TRUE(portal_watchdog_fired(PORTAL_WATCHDOG_MS + MINUTE, 0));
}

TEST(PortalWatchdog, ActivityKeepsThePortalAlive)
{
    /*
    Человек, который настраивает час, час и настраивает: каждое действие
    начинает отсчёт заново, и никакого общего предела над ним нет.
    */
    for (uint32_t action = 0; action < 2 * 60 * MINUTE; action += 9 * MINUTE)
    {
        EXPECT_FALSE(portal_watchdog_fired(action + 9 * MINUTE, action))
            << "действие на " << action / MINUTE << " минуте не продлило окно";
    }
}

TEST(PortalWatchdog, IdleWindowMatchesAttiny)
{
    /*
    Питание снимает attiny: после команды 'E' он держит ЕСП ровно
    SETUP_TIME_MSEC (Attiny85/src/Setup.h). Если окна разъедутся, продление
    станет бессмысленным — одна сторона выключится раньше другой.
    */
    EXPECT_EQ(PORTAL_WATCHDOG_MS, 600000UL);
}

TEST(PortalWatchdog, MillisOverflowDoesNotCutSetupShort)
{
    /*
    millis() переполняется через 49 суток. Момент сам по себе ничего не
    значит: если сравнивать моменты напрямую, портал после переполнения
    закрылся бы мгновенно.
    */
    const uint32_t fed = 0xFFFFF000UL;         // до переполнения осталось ~4 с
    const uint32_t now = fed + 5 * MINUTE;     // счётчик уже перевалил через ноль

    EXPECT_LT(now, fed);   // проверяем, что тест действительно про переполнение
    EXPECT_FALSE(portal_watchdog_fired(now, fed));
    EXPECT_TRUE(portal_watchdog_fired(fed + PORTAL_WATCHDOG_MS, fed));
}

TEST(PortalWatchdog, SecondsLeftCountDown)
{
    /*
    Остаток нужен снаружи: человеку — чтобы понять, почему портал вот-вот
    закроется, проверке — чтобы увидеть продление сразу, а не через десять
    минут ожидания.
    */
    EXPECT_EQ(portal_idle_seconds_left(0, 0), PORTAL_WATCHDOG_MS / 1000);
    EXPECT_EQ(portal_idle_seconds_left(4 * MINUTE, 0), 6 * 60UL);
}

TEST(PortalWatchdog, FeedingRestoresTheWindow)
{
    // Действие пользователя двигает точку отсчёта, а не остаток
    const uint32_t fed = 7 * MINUTE;

    EXPECT_EQ(portal_idle_seconds_left(fed, 0), 3 * 60UL);
    EXPECT_EQ(portal_idle_seconds_left(fed, fed), PORTAL_WATCHDOG_MS / 1000);
}

TEST(PortalWatchdog, ExpiredMeansZeroNotWrapAround)
{
    /*
    Истёкший срок обязан быть нулём: на беззнаковой разности «минус секунда»
    превратилась бы в 49 суток, и страница показала бы, что времени полно.
    */
    EXPECT_EQ(portal_idle_seconds_left(PORTAL_WATCHDOG_MS, 0), 0UL);
    EXPECT_EQ(portal_idle_seconds_left(PORTAL_WATCHDOG_MS + MINUTE, 0), 0UL);
}
