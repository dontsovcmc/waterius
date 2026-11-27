#include <gtest/gtest.h>
#include "core/timekeeping.h"

/*
Решение "пора ли синхронизировать время" (#357).

Раньше NTP опрашивался при каждом пробуждении: на 15-минутном периоде это
35 тысяч запросов в год на устройство. Теперь не чаще раза в 45 минут, а
между синхронизациями время считается по счётчику пробуждений.

Сутки оказались слишком редко: attiny принимает период в целых минутах, и
ошибка округления поправки за сутки уводила выход на связь на 12 минут при
часовом периоде. Довести до расписания можно только в сеансе с синхронизацией.

Счётчик пробуждений нужен не только для срока: по нему считается поправка
частоты attiny. Знать точное число проспанных периодов — единственный
способ отличить "attiny спешит" от "не было интернета три периода подряд".
*/

namespace
{
    const time_t SYNCED = 1755000000;   // достоверное время последней синхронизации
    const uint16_t DAILY = 1440;        // период по умолчанию, мин
    const uint16_t HOURLY = 60;         // длиннее интервала: синхронизация каждое пробуждение
    const uint16_t HALF = 30;           // срок 2 пробуждения
    const uint16_t QUARTER = 15;        // типичный период для Home Assistant, срок 3 пробуждения
    const uint8_t SETTLED = NTP_WARMUP_SYNCS;   // поправка уже измерена
}

// --- срок синхронизации ---

TEST(NeedSync, FirstEverSyncHappensImmediately)
{
    // last_time_sync == 0: устройство ни разу не знало времени
    EXPECT_TRUE(need_ntp_sync(0, 0, 0, DAILY, SETTLED));
    EXPECT_TRUE(need_ntp_sync(0, 1, 0, DAILY, SETTLED));
}

TEST(NeedSync, BogusOldTimeCountsAsNeverSynced)
{
    EXPECT_TRUE(need_ntp_sync(1600000000, 3, 0, DAILY, SETTLED));   // 2020 год
}

TEST(NeedSync, NotBeforeIntervalPasses)
{
    // 15 минут: 45 минут — это 3 пробуждения
    for (uint16_t w = 0; w < 3; ++w)
        EXPECT_FALSE(need_ntp_sync(SYNCED, w, 0, QUARTER, SETTLED)) << "пробуждение " << w;

    EXPECT_TRUE(need_ntp_sync(SYNCED, 3, 0, QUARTER, SETTLED));
}

TEST(NeedSync, HalfHourSyncsEverySecondWakeup)
{
    // Срок округляется вверх: 45 / 30 = 1.5 -> 2 пробуждения, не 1
    EXPECT_FALSE(need_ntp_sync(SYNCED, 1, 0, HALF, SETTLED));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 2, 0, HALF, SETTLED));
}

TEST(NeedSync, HourlyPeriodSyncsEveryWakeup)
{
    // Часовой период: каждый сеанс доводит до точки расписания, и ошибка
    // округления поправки не копится между сеансами
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, HOURLY, SETTLED));
}

TEST(NeedSync, DailyPeriodSyncsEveryWakeup)
{
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, DAILY, SETTLED));
}

TEST(NeedSync, PeriodLongerThanIntervalSyncsEveryWakeup)
{
    // Период 30 суток длиннее интервала: синхронизируемся каждое пробуждение,
    // иначе поправка частоты будет считаться по устаревшему времени
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, 43200, SETTLED));
}

TEST(NeedSync, ZeroPeriodDoesNotDivideByZero)
{
    EXPECT_TRUE(need_ntp_sync(SYNCED, 20000, 0, 0, SETTLED));
}

// --- отступ после неудач (пункт 4 issue: не долбить NTP) ---

TEST(Backoff, FirstFailureWaitsTwoWakeups)
{
    // Срок 3 пробуждения, попытка на 3-м не удалась.
    // Следующая попытка не раньше 3 + 2
    EXPECT_FALSE(need_ntp_sync(SYNCED, 4, 1, QUARTER, SETTLED));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 5, 1, QUARTER, SETTLED));

    // Часовой период синхронизируется каждое пробуждение, но после неудачи
    // тоже ждёт: лежащий роутер не должен стоить запроса на каждом сеансе
    EXPECT_FALSE(need_ntp_sync(SYNCED, 2, 1, HOURLY, SETTLED));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 3, 1, HOURLY, SETTLED));
}

TEST(Backoff, PauseDoublesWithEachFailure)
{
    // 3 + 2 = 5, 5 + 4 = 9, 9 + 8 = 17, 17 + 16 = 33
    EXPECT_TRUE(need_ntp_sync(SYNCED, 9, 2, QUARTER, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, 8, 2, QUARTER, SETTLED));

    EXPECT_TRUE(need_ntp_sync(SYNCED, 17, 3, QUARTER, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, 16, 3, QUARTER, SETTLED));

    EXPECT_TRUE(need_ntp_sync(SYNCED, 33, 4, QUARTER, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, 32, 4, QUARTER, SETTLED));
}

TEST(Backoff, PauseStopsGrowing)
{
    // Иначе устройство, у которого месяц нет интернета, замолчало бы
    // навсегда: 2^20 пробуждений — это дольше срока службы батареи.
    // Шаг ограничен 64 пробуждениями.
    const uint8_t many = 40;
    const uint16_t capped = 3 + 2 + 4 + 8 + 16 + 32 + 64 * (many - 5);

    EXPECT_TRUE(need_ntp_sync(SYNCED, capped, many, QUARTER, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, capped - 1, many, QUARTER, SETTLED));
}

TEST(Backoff, SuccessResetsPause)
{
    // Счётчик неудач обнуляется при успехе, и следующий срок — обычный
    EXPECT_TRUE(need_ntp_sync(SYNCED, 3, 0, QUARTER, SETTLED));
}

TEST(Backoff, AppliesToNeverSyncedDeviceToo)
{
    // Устройство без интернета с завода тоже не должно долбить NTP
    EXPECT_FALSE(need_ntp_sync(0, 1, 1, DAILY, SETTLED));
    EXPECT_TRUE(need_ntp_sync(0, 2, 1, DAILY, SETTLED));
}

// --- счётчик пробуждений ---

TEST(BumpWakeups, CountsUp)
{
    EXPECT_EQ(bump_wakeups(0), 1);
    EXPECT_EQ(bump_wakeups(671), 672);
}

TEST(BumpWakeups, SaturatesInsteadOfWrapping)
{
    // Переполнение обнулило бы число проспанных периодов, и поправка
    // частоты attiny стала бы считаться по заниженному делителю
    EXPECT_EQ(bump_wakeups(65535), 65535);
    EXPECT_EQ(bump_wakeups(65534), 65535);
}

// --- оценка времени между синхронизациями ---

TEST(EstimateTime, AddsWholePeriods)
{
    EXPECT_EQ(estimate_time(SYNCED, 1, DAILY), SYNCED + 1440 * 60);
    EXPECT_EQ(estimate_time(SYNCED, 7, DAILY), SYNCED + 7 * 1440 * 60);
}

TEST(EstimateTime, RightAfterSyncReturnsSyncTime)
{
    EXPECT_EQ(estimate_time(SYNCED, 0, DAILY), SYNCED);
}

TEST(EstimateTime, StaysValidForLongOfflinePeriod)
{
    // Полгода без интернета на 15-минутном периоде: оценка не должна
    // переполниться и уехать в прошлое
    const time_t estimated = estimate_time(SYNCED, 17520, QUARTER);

    EXPECT_GT(estimated, SYNCED);
    EXPECT_TRUE(is_valid_time(estimated));
}

TEST(EstimateTime, UnknownSyncTimeGivesNothing)
{
    // Оценивать не от чего: у вызывающего кода остаётся clock_before_sync
    EXPECT_EQ(estimate_time(0, 5, DAILY), 0);
    EXPECT_EQ(estimate_time(1600000000, 5, DAILY), 0);
}


// --- прогрев: первая поправка не должна ждать обычного срока ---

TEST(Warmup, SyncsEveryWakeupUntilDriftIsMeasured)
{
    // Поправка частоты attiny считается по паре синхронизаций. Пока пары нет,
    // устройство идёт с period_min_tuned из настройки и уезжает от расписания.
    // Поэтому первые NTP_WARMUP_SYNCS раз синхронизируемся каждое пробуждение.
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, QUARTER, 0));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, QUARTER, 1));
}

TEST(Warmup, RegularScheduleStartsAfterWarmup)
{
    EXPECT_FALSE(need_ntp_sync(SYNCED, 1, 0, QUARTER, NTP_WARMUP_SYNCS));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 3, 0, QUARTER, NTP_WARMUP_SYNCS));
}

TEST(Warmup, BackoffAppliesDuringWarmupToo)
{
    // Даже на прогреве неудачи не должны превращаться в опрос NTP
    // при каждом пробуждении
    // Срок на прогреве — 1 пробуждение, после неудачи ждём ещё 2
    EXPECT_FALSE(need_ntp_sync(SYNCED, 2, 1, QUARTER, 0));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 3, 1, QUARTER, 0));
}

TEST(Warmup, CounterAboveThresholdIsStillSettled)
{
    // Счётчик насыщается, но на всякий случай: любое значение выше порога
    // означает "прогрев закончен"
    EXPECT_FALSE(need_ntp_sync(SYNCED, 1, 0, QUARTER, 200));
}
