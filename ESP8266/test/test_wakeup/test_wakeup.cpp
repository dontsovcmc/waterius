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

// --- долгий обрыв связи ---

TEST(Wakeup, OfflineFor48HoursWithDailyPeriod)
{
    // Период сутки, устройство молчало двое суток: сервер лежал или роутер
    // был выключен.
    //
    // Итог 1178 вместо 1440: k считается как 2880/1296 = 2.222, целая часть
    // отбрасывается, и дробные 0.222 принимаются за уход частоты attiny —
    // хотя это просто остаток от пропущенных периодов. Время выхода на связь
    // сдвигается примерно на 4 часа и сходится обратно за несколько циклов.
    // Характеризация: так работает сегодня (родня #345, #347).
    WakeupTune tune = tune_wakeup(BASE + 2880 * MIN, BASE, BASE, 1440, 1296);

    EXPECT_EQ(tune.period_min_tuned, 1178);
    EXPECT_DOUBLE_EQ(tune.slept_min, 2880.0);
}

TEST(Wakeup, OfflineFor48HoursWithHourlyPeriod)
{
    // Тот же обрыв, но период час: 2880/60 = 48.0 ровно, дробного остатка
    // нет — и поправка не портится вовсе.
    WakeupTune tune = tune_wakeup(BASE + 2880 * MIN, BASE, BASE, 60, 60);

    EXPECT_EQ(tune.period_min_tuned, 60);
}

TEST(Wakeup, OfflineForMonthShiftsScheduleButSurvives)
{
    // 43200/1296 = 33.33 — та же дробь, сдвиг около 6 часов
    WakeupTune tune = tune_wakeup(BASE + 43200L * MIN, BASE, BASE, 1440, 1296);

    EXPECT_EQ(tune.period_min_tuned, 1080);
    EXPECT_GT(tune.period_min_tuned, 0);
}

// --- attiny перезагрузился ---

TEST(Wakeup, AttinyRebootWakesEspMuchEarlier)
{
    // При перезагрузке attiny теряет заказанный период и будит ESP через
    // свой период по умолчанию (#350): вместо 1296 минут сон вышел 15.
    // Алгоритм всё равно целится в суточную отметку — 1409 минут.
    WakeupTune tune = tune_wakeup(BASE + 15 * MIN, BASE, BASE, 1440, 1296);

    EXPECT_EQ(tune.period_min_tuned, 1409);
}

TEST(Wakeup, AttinyRebootDoesNotPoisonCorrection)
{
    // Ранний выход на связь читается как "attiny спешит на 25%", поэтому
    // на 45 реальных минут заказывается 36 кривых. Ошибка небольшая и
    // уходит на следующем цикле — в мусор поправка не превращается.
    WakeupTune tune = tune_wakeup(BASE + 15 * MIN, BASE, BASE, 60, 60);

    EXPECT_EQ(tune.period_min_tuned, 36);
}

// --- время ушло назад ---

TEST(Wakeup, ClockMovedBackwardsAfterNtp)
{
    // NTP может перевести часы назад: до синхронизации время было выдуманным.
    // Тогда now < last_send и сон получается отрицательным.
    //
    // Отрицательный сон ничего не говорит о частоте attiny, поэтому поправка
    // остаётся нейтральной и устройство просто целится в следующую точку
    // расписания. Раньше здесь было неопределённое поведение: приведение
    // отрицательного double к uint16_t.
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE + 120 * MIN, 60, 60);

    EXPECT_EQ(tune.period_min_tuned, 60);
    EXPECT_DOUBLE_EQ(tune.slept_min, -120.0);
}

TEST(Wakeup, ClockMovedBackwardsADay)
{
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE + 1440 * MIN, 1440, 1296);

    EXPECT_EQ(tune.period_min_tuned, 1440);
}

// --- длинные периоды: результат обязан влезать в uint16_t ---

TEST(Wakeup, ThirtyDayPeriodFitsWithoutClamp)
{
    // 30 суток — законный период, до предела типа ещё есть запас
    const uint16_t PER = 43200;
    WakeupTune tune = tune_wakeup(BASE + 60, BASE, BASE + 60 - 30240L * MIN, PER, 43200);

    EXPECT_EQ(tune.period_min_tuned, 61713);
}

TEST(Wakeup, PeriodBeyond32DaysFallsBackInsteadOfWrapping)
{
    // 47000 минут (32.6 суток): деление на k = 0.7 даёт 67141, что не влезает
    // в uint16_t. Раньше приведение оборачивалось в 1605 — устройство
    // просыпалось через сутки вместо 46. Теперь откат к номинальному периоду.
    const uint16_t PER = 47000;
    WakeupTune tune = tune_wakeup(BASE + 60, BASE, BASE + 60 - 32900L * MIN, PER, 47000);

    EXPECT_EQ(tune.period_min_tuned, PER);
}

TEST(Wakeup, MaximumPeriodFallsBack)
{
    const uint16_t PER = 65535;
    WakeupTune tune = tune_wakeup(BASE + 60, BASE, BASE + 60 - 45874L * MIN, PER, 65530);

    EXPECT_EQ(tune.period_min_tuned, PER);
}

TEST(Wakeup, HugeSleepDoesNotOverflowCorrection)
{
    // Сон, деление которого на период даёт k больше 65535: приведение к
    // uint16_t было бы неопределённым, floor — нет
    WakeupTune tune = tune_wakeup(BASE + 900000L * MIN, BASE, BASE, 60, 13);

    EXPECT_GT(tune.period_min_tuned, 0);
    EXPECT_LE(tune.period_min_tuned, 65535);
}
