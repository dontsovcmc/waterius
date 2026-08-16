#include <gtest/gtest.h>
#include "core/wakeup.h"

/*
Тесты фиксируют текущее поведение подстройки периода пробуждения.

Единицы измерения: period_min_tuned — это "кривые минуты attiny", то есть
сколько минут заказать сторожевому таймеру. Реальное время сна отличается,
потому что частота watchdog у каждого attiny своя. Задача алгоритма —
выходить на связь в одно и то же время суток, отсчитывая от base_time.
*/

namespace
{
    const time_t BASE = 1700000000;   // время настройки пользователем
    const long MIN = 60;              // секунд в минуте
}

// --- нормальная работа ---

TEST(Wakeup, PerfectSleepKeepsPeriod)
{
    // Заказали 60, проспали ровно 60 реальных минут — attiny точен, k = 1.
    WakeupTune tune = tune_wakeup(BASE + 60 * MIN, BASE, BASE, 60, 60);

    EXPECT_EQ(tune.period_min_tuned, 60);
    EXPECT_DOUBLE_EQ(tune.slept_min, 60.0);
}

TEST(Wakeup, AttinyRunsFastOrdersMoreMinutes)
{
    // Заказали 60, проспали 54 реальных минуты: минута attiny короче реальной.
    // Чтобы попасть в следующую точку, надо заказать больше кривых минут,
    // чем до неё реальных.
    WakeupTune tune = tune_wakeup(BASE + 54 * MIN, BASE, BASE, 60, 60);

    EXPECT_EQ(tune.period_min_tuned, 73);   // до цели 66 реальных минут / k=0.9
    EXPECT_GT(tune.period_min_tuned, 66);
}

TEST(Wakeup, AttinyRunsSlowOrdersFewerMinutes)
{
    // Заказали 60, проспали 66: минута attiny длиннее реальной.
    WakeupTune tune = tune_wakeup(BASE + 66 * MIN, BASE, BASE, 60, 60);

    EXPECT_EQ(tune.period_min_tuned, 49);   // до цели 54 реальных минуты / k=1.1
    EXPECT_LT(tune.period_min_tuned, 54);
}

// --- нештатные ситуации ---

TEST(Wakeup, MissedWakeupsDoNotBreakCorrection)
{
    // Не было интернета, устройство молчало три периода подряд.
    // Дробная часть k отбрасывается, поправка не уезжает в мусор.
    WakeupTune tune = tune_wakeup(BASE + 180 * MIN, BASE, BASE, 60, 60);

    EXPECT_EQ(tune.period_min_tuned, 60);
}

TEST(Wakeup, MissedWakeupsWithDriftKeepFraction)
{
    // Проспали два периода, при этом attiny отстаёт: 2 * 66 = 132 минуты.
    WakeupTune tune = tune_wakeup(BASE + 132 * MIN, BASE, BASE, 60, 60);

    EXPECT_GT(tune.period_min_tuned, 0);
    EXPECT_LT(tune.period_min_tuned, 60);   // поправка учитывает отставание
}

TEST(Wakeup, ZeroTunedPeriodDoesNotDivideByZero)
{
    // period_min_tuned == 0 — конфиг ещё не подстроен.
    WakeupTune tune = tune_wakeup(BASE + 60 * MIN, BASE, BASE, 60, 0);

    EXPECT_EQ(tune.period_min_tuned, 60);
}

TEST(Wakeup, NoSleepAtAllDoesNotDivideByZero)
{
    // now == last_send: проснулись мгновенно (ручное пробуждение кнопкой).
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE, 60, 60);

    EXPECT_GT(tune.period_min_tuned, 0);
    EXPECT_LE(tune.period_min_tuned, 60);
}

// --- защита от "ловушки" у самой цели ---

TEST(Wakeup, TargetCloserThanOneMinuteSkipsToNextSlot)
{
    // До целевой точки 0.1 минуты — целиться в неё бессмысленно,
    // берём следующую.
    time_t now = BASE + 120 * MIN - 6;   // 6 секунд до цели
    WakeupTune tune = tune_wakeup(now, BASE, BASE, 60, 60);

    EXPECT_GT(tune.period_min_tuned, 30);   // не 0 и не доли минуты
}

TEST(Wakeup, TargetCloserThan30PercentSkipsToNextSlot)
{
    // До цели 6 минут при периоде 60 — это меньше 30%, целимся в следующую.
    WakeupTune tune = tune_wakeup(BASE + 54 * MIN, BASE, BASE, 60, 60);

    EXPECT_GT(tune.period_min_tuned, 60);
}

TEST(Wakeup, TargetJustOver30PercentIsKept)
{
    // До цели 20 минут при периоде 60 — больше 30%, цель не переносим.
    WakeupTune tune = tune_wakeup(BASE + 40 * MIN, BASE, BASE, 60, 60);

    EXPECT_LT(tune.period_min_tuned, 60);
}

// --- разные периоды ---

TEST(Wakeup, Period15Min)
{
    WakeupTune tune = tune_wakeup(BASE + 15 * MIN, BASE, BASE, 15, 15);
    EXPECT_EQ(tune.period_min_tuned, 15);
}

TEST(Wakeup, Period720Min)
{
    WakeupTune tune = tune_wakeup(BASE + 720 * MIN, BASE, BASE, 720, 720);
    EXPECT_EQ(tune.period_min_tuned, 720);
}

TEST(Wakeup, Period1440MinFitsUint16)
{
    // Сутки — максимальный период. Даже с максимальной поправкой (k = 0.7)
    // результат обязан влезать в uint16_t, иначе устройство проснётся не тогда.
    WakeupTune tune = tune_wakeup(BASE + 1440 * MIN * 7 / 10, BASE, BASE, 1440, 1440);

    EXPECT_GT(tune.period_min_tuned, 0);
    EXPECT_LT(tune.period_min_tuned, 65535);
}

TEST(Wakeup, ResultIsAlwaysPositiveAcrossSchedule)
{
    // Проходим сутки с шагом в минуту: период не должен обнуляться никогда —
    // ноль означает, что attiny больше не разбудит ESP.
    for (long m = 0; m <= 1440; ++m)
    {
        WakeupTune tune = tune_wakeup(BASE + m * MIN, BASE, BASE, 60, 60);
        ASSERT_GT(tune.period_min_tuned, 0) << "минута " << m;
    }
}

// --- сброс после смены периода пользователем ---

TEST(Wakeup, UserChangeGives90PercentOfPeriod)
{
    EXPECT_EQ(period_after_user_change(60), 54);
    EXPECT_EQ(period_after_user_change(1440), 1296);
}

TEST(Wakeup, UserChangeTruncatesTowardsZero)
{
    // 15 * 0.9 = 13.5, приведение к uint16_t отбрасывает дробную часть
    EXPECT_EQ(period_after_user_change(15), 13);
}
