#include <gtest/gtest.h>
#include "core/timekeeping.h"

/*
Решение "пора ли синхронизировать время" (#357).

Раньше NTP опрашивался при каждом пробуждении: на 15-минутном периоде это
35 тысяч запросов в год на устройство. Теперь раз в сутки, а между
синхронизациями время считается по счётчику пробуждений.

Счётчик пробуждений нужен не только для срока: по нему считается поправка
частоты attiny. Знать точное число проспанных периодов — единственный
способ отличить "attiny спешит" от "не было интернета три периода подряд".
*/

namespace
{
    const time_t SYNCED = 1755000000;   // достоверное время последней синхронизации
    const uint16_t DAILY = 1440;        // период по умолчанию, мин
    const uint16_t HOURLY = 60;         // период, на котором виден срок: 24 пробуждения
    const uint16_t QUARTER = 15;        // типичный период для Home Assistant
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
    // Часовой период: сутки — это 24 пробуждения
    for (uint16_t w = 0; w < 24; ++w)
        EXPECT_FALSE(need_ntp_sync(SYNCED, w, 0, HOURLY, SETTLED)) << "пробуждение " << w;

    EXPECT_TRUE(need_ntp_sync(SYNCED, 24, 0, HOURLY, SETTLED));
}

TEST(NeedSync, PeriodEqualToIntervalSyncsEveryWakeup)
{
    // Суточный период: реже раза в сутки не получится при всём желании,
    // и это ровно тот случай, когда точность расписания важнее экономии
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, DAILY, SETTLED));
}

TEST(NeedSync, ShortPeriodMeansMoreWakeupsPerInterval)
{
    // 15 минут: сутки — это 96 пробуждений. Именно здесь экономия и живёт:
    // 96 запросов к NTP в сутки превращаются в один
    EXPECT_FALSE(need_ntp_sync(SYNCED, 95, 0, QUARTER, SETTLED));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 96, 0, QUARTER, SETTLED));
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
    // Срок 24 пробуждения, попытка на 24-м не удалась.
    // Следующая попытка не раньше 24 + 2
    EXPECT_FALSE(need_ntp_sync(SYNCED, 25, 1, HOURLY, SETTLED));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 26, 1, HOURLY, SETTLED));
}

TEST(Backoff, PauseDoublesWithEachFailure)
{
    // 24 + 2 = 26, 26 + 4 = 30, 30 + 8 = 38, 38 + 16 = 54
    EXPECT_TRUE(need_ntp_sync(SYNCED, 30, 2, HOURLY, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, 29, 2, HOURLY, SETTLED));

    EXPECT_TRUE(need_ntp_sync(SYNCED, 38, 3, HOURLY, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, 37, 3, HOURLY, SETTLED));

    EXPECT_TRUE(need_ntp_sync(SYNCED, 54, 4, HOURLY, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, 53, 4, HOURLY, SETTLED));
}

TEST(Backoff, PauseStopsGrowing)
{
    // Иначе устройство, у которого месяц нет интернета, замолчало бы
    // навсегда: 2^20 пробуждений — это дольше срока службы батареи.
    // Шаг ограничен 64 пробуждениями.
    const uint8_t many = 40;
    const uint16_t capped = 24 + 2 + 4 + 8 + 16 + 32 + 64 * (many - 5);

    EXPECT_TRUE(need_ntp_sync(SYNCED, capped, many, HOURLY, SETTLED));
    EXPECT_FALSE(need_ntp_sync(SYNCED, capped - 1, many, HOURLY, SETTLED));
}

TEST(Backoff, SuccessResetsPause)
{
    // Счётчик неудач обнуляется при успехе, и следующий срок — обычный
    EXPECT_TRUE(need_ntp_sync(SYNCED, 24, 0, HOURLY, SETTLED));
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


// --- прогрев: первая поправка не должна ждать неделю ---

TEST(Warmup, SyncsEveryWakeupUntilDriftIsMeasured)
{
    // Поправка частоты attiny считается по паре синхронизаций. Пока пары нет,
    // устройство идёт с period_min_tuned из настройки и уезжает от расписания.
    // Поэтому первые NTP_WARMUP_SYNCS раз синхронизируемся каждое пробуждение.
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, HOURLY, 0));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 1, 0, HOURLY, 1));
}

TEST(Warmup, RegularScheduleStartsAfterWarmup)
{
    EXPECT_FALSE(need_ntp_sync(SYNCED, 1, 0, HOURLY, NTP_WARMUP_SYNCS));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 24, 0, HOURLY, NTP_WARMUP_SYNCS));
}

TEST(Warmup, BackoffAppliesDuringWarmupToo)
{
    // Даже на прогреве неудачи не должны превращаться в опрос NTP
    // при каждом пробуждении
    // Срок на прогреве — 1 пробуждение, после неудачи ждём ещё 2
    EXPECT_FALSE(need_ntp_sync(SYNCED, 2, 1, HOURLY, 0));
    EXPECT_TRUE(need_ntp_sync(SYNCED, 3, 1, HOURLY, 0));
}

TEST(Warmup, CounterAboveThresholdIsStillSettled)
{
    // Счётчик насыщается, но на всякий случай: любое значение выше порога
    // означает "прогрев закончен"
    EXPECT_FALSE(need_ntp_sync(SYNCED, 1, 0, HOURLY, 200));
}
