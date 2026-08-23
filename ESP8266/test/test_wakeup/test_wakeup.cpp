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
    WakeupTune tune = tune_wakeup(BASE + 60 * MIN, BASE, BASE, 60, 60, 1);

    EXPECT_EQ(tune.period_min_tuned, 60);
    EXPECT_DOUBLE_EQ(tune.slept_min, 60.0);
}

TEST(Wakeup, AttinyRunsFastOrdersMoreMinutes)
{
    // Заказали 60, проспали 54 реальных минуты: минута attiny короче реальной.
    // Чтобы попасть в следующую точку, надо заказать больше кривых минут,
    // чем до неё реальных.
    WakeupTune tune = tune_wakeup(BASE + 54 * MIN, BASE, BASE, 60, 60, 1);

    EXPECT_EQ(tune.period_min_tuned, 73);   // до цели 66 реальных минут / k=0.9
    EXPECT_GT(tune.period_min_tuned, 66);
}

TEST(Wakeup, AttinyRunsSlowOrdersFewerMinutes)
{
    // Заказали 60, проспали 66: минута attiny длиннее реальной.
    WakeupTune tune = tune_wakeup(BASE + 66 * MIN, BASE, BASE, 60, 60, 1);

    EXPECT_EQ(tune.period_min_tuned, 49);   // до цели 54 реальных минуты / k=1.1
    EXPECT_LT(tune.period_min_tuned, 54);
}

// --- нештатные ситуации ---

TEST(Wakeup, MissedWakeupsDoNotBreakCorrection)
{
    // Не было интернета: устройство просыпалось три раза, но время узнало
    // только сейчас. Три пробуждения по 60 минут — attiny точен, k = 1.
    WakeupTune tune = tune_wakeup(BASE + 180 * MIN, BASE, BASE, 60, 60, 3);

    EXPECT_EQ(tune.period_min_tuned, 60);
}

TEST(Wakeup, MissedWakeupsWithDriftKeepFraction)
{
    // Проспали два периода, при этом attiny отстаёт: 2 * 66 = 132 минуты.
    // k = 132 / (60 * 2) = 1.1 — ровно то же отставание, что и на одном
    // периоде, число пропусков на поправку не влияет.
    WakeupTune tune = tune_wakeup(BASE + 132 * MIN, BASE, BASE, 60, 60, 2);

    EXPECT_EQ(tune.period_min_tuned, 44);
    EXPECT_LT(tune.period_min_tuned, 60);   // поправка учитывает отставание
}

TEST(Wakeup, ZeroTunedPeriodDoesNotDivideByZero)
{
    // period_min_tuned == 0 — конфиг ещё не подстроен.
    WakeupTune tune = tune_wakeup(BASE + 60 * MIN, BASE, BASE, 60, 0, 1);

    EXPECT_EQ(tune.period_min_tuned, 60);
}

TEST(Wakeup, NoSleepAtAllDoesNotDivideByZero)
{
    // now == last_send: проснулись мгновенно (ручное пробуждение кнопкой).
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE, 60, 60, 1);

    EXPECT_GT(tune.period_min_tuned, 0);
    EXPECT_LE(tune.period_min_tuned, 60);
}

// --- защита от "ловушки" у самой цели ---

TEST(Wakeup, TargetCloserThanOneMinuteSkipsToNextSlot)
{
    // До целевой точки 0.1 минуты — целиться в неё бессмысленно,
    // берём следующую.
    time_t now = BASE + 120 * MIN - 6;   // 6 секунд до цели
    WakeupTune tune = tune_wakeup(now, BASE, BASE, 60, 60, 1);

    EXPECT_GT(tune.period_min_tuned, 30);   // не 0 и не доли минуты
}

TEST(Wakeup, TargetCloserThan30PercentSkipsToNextSlot)
{
    // До цели 6 минут при периоде 60 — это меньше 30%, целимся в следующую.
    WakeupTune tune = tune_wakeup(BASE + 54 * MIN, BASE, BASE, 60, 60, 1);

    EXPECT_GT(tune.period_min_tuned, 60);
}

TEST(Wakeup, TargetJustOver30PercentIsKept)
{
    // До цели 20 минут при периоде 60 — больше 30%, цель не переносим.
    WakeupTune tune = tune_wakeup(BASE + 40 * MIN, BASE, BASE, 60, 60, 1);

    EXPECT_LT(tune.period_min_tuned, 60);
}

// --- разные периоды ---

TEST(Wakeup, Period15Min)
{
    WakeupTune tune = tune_wakeup(BASE + 15 * MIN, BASE, BASE, 15, 15, 1);
    EXPECT_EQ(tune.period_min_tuned, 15);
}

TEST(Wakeup, Period720Min)
{
    WakeupTune tune = tune_wakeup(BASE + 720 * MIN, BASE, BASE, 720, 720, 1);
    EXPECT_EQ(tune.period_min_tuned, 720);
}

TEST(Wakeup, Period1440MinFitsUint16)
{
    // Сутки — максимальный период. Даже с максимальной поправкой (k = 0.7)
    // результат обязан влезать в uint16_t, иначе устройство проснётся не тогда.
    WakeupTune tune = tune_wakeup(BASE + 1440 * MIN * 7 / 10, BASE, BASE, 1440, 1440, 1);

    EXPECT_GT(tune.period_min_tuned, 0);
    EXPECT_LT(tune.period_min_tuned, 65535);
}

TEST(Wakeup, ResultIsAlwaysPositiveAcrossSchedule)
{
    // Проходим сутки с шагом в минуту: период не должен обнуляться никогда —
    // ноль означает, что attiny больше не разбудит ESP.
    for (long m = 0; m <= 1440; ++m)
    {
        WakeupTune tune = tune_wakeup(BASE + m * MIN, BASE, BASE, 60, 60, 1);
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
    // Период сутки, устройство двое суток не могло выйти на связь: сервер
    // лежал или роутер был выключен. Проснулось оно при этом дважды.
    //
    // Раньше здесь получалось 1178 вместо 1440: k считался как 2880/1296 =
    // 2.222, целая часть отбрасывалась, и дробные 0.222 принимались за уход
    // частоты attiny — хотя это был остаток от пропущенных периодов. Время
    // выхода на связь уезжало на 4 часа (#345, #347).
    //
    // Со счётчиком пробуждений k = 2880 / (1296 * 2) = 1.111 — это настоящая
    // поправка, и заказ 1296 кривых минут снова даёт сутки реальных.
    WakeupTune tune = tune_wakeup(BASE + 2880 * MIN, BASE, BASE, 1440, 1296, 2);

    EXPECT_EQ(tune.period_min_tuned, 1296);
    EXPECT_DOUBLE_EQ(tune.slept_min, 2880.0);
}

TEST(Wakeup, OfflineFor48HoursWithHourlyPeriod)
{
    // Тот же обрыв, но период час: 48 пробуждений по 60 минут.
    WakeupTune tune = tune_wakeup(BASE + 2880 * MIN, BASE, BASE, 60, 60, 48);

    EXPECT_EQ(tune.period_min_tuned, 60);
}

TEST(Wakeup, OfflineForMonthKeepsSchedule)
{
    // Месяц без связи — 30 пробуждений. Раньше выходило 1080 (сдвиг около
    // 6 часов), теперь поправка не зависит от длины обрыва.
    WakeupTune tune = tune_wakeup(BASE + 43200L * MIN, BASE, BASE, 1440, 1296, 30);

    EXPECT_EQ(tune.period_min_tuned, 1296);
}

// --- attiny перезагрузился ---

TEST(Wakeup, AttinyRebootWakesEspMuchEarlier)
{
    // При перезагрузке attiny теряет заказанный период и будит ESP через
    // свой период по умолчанию (#350): вместо 1296 минут сон вышел 15.
    // Алгоритм всё равно целится в суточную отметку — 1409 минут.
    WakeupTune tune = tune_wakeup(BASE + 15 * MIN, BASE, BASE, 1440, 1296, 1);

    EXPECT_EQ(tune.period_min_tuned, 1409);
}

TEST(Wakeup, AttinyRebootDoesNotPoisonCorrection)
{
    // Ранний выход на связь читается как "attiny спешит на 25%", поэтому
    // на 45 реальных минут заказывается 36 кривых. Ошибка небольшая и
    // уходит на следующем цикле — в мусор поправка не превращается.
    WakeupTune tune = tune_wakeup(BASE + 15 * MIN, BASE, BASE, 60, 60, 1);

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
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE + 120 * MIN, 60, 60, 1);

    EXPECT_EQ(tune.period_min_tuned, 60);
    EXPECT_DOUBLE_EQ(tune.slept_min, -120.0);
}

TEST(Wakeup, ClockMovedBackwardsADay)
{
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE + 1440 * MIN, 1440, 1296, 1);

    EXPECT_EQ(tune.period_min_tuned, 1440);
}

// --- длинные периоды: результат обязан влезать в uint16_t ---

TEST(Wakeup, ThirtyDayPeriodFitsWithoutClamp)
{
    // 30 суток — законный период, до предела типа ещё есть запас
    const uint16_t PER = 43200;
    WakeupTune tune = tune_wakeup(BASE + 60, BASE, BASE + 60 - 30240L * MIN, PER, 43200, 1);

    EXPECT_EQ(tune.period_min_tuned, 61713);
}

TEST(Wakeup, PeriodBeyond32DaysFallsBackInsteadOfWrapping)
{
    // 47000 минут (32.6 суток): деление на k = 0.7 даёт 67141, что не влезает
    // в uint16_t. Раньше приведение оборачивалось в 1605 — устройство
    // просыпалось через сутки вместо 46. Теперь откат к номинальному периоду.
    const uint16_t PER = 47000;
    WakeupTune tune = tune_wakeup(BASE + 60, BASE, BASE + 60 - 32900L * MIN, PER, 47000, 1);

    EXPECT_EQ(tune.period_min_tuned, PER);
}

TEST(Wakeup, MaximumPeriodFallsBack)
{
    const uint16_t PER = 65535;
    WakeupTune tune = tune_wakeup(BASE + 60, BASE, BASE + 60 - 45874L * MIN, PER, 65530, 1);

    EXPECT_EQ(tune.period_min_tuned, PER);
}

TEST(Wakeup, HugeSleepDoesNotOverflowCorrection)
{
    // Сон, деление которого на период даёт k больше 65535: приведение к
    // uint16_t было бы неопределённым, floor — нет
    WakeupTune tune = tune_wakeup(BASE + 900000L * MIN, BASE, BASE, 60, 13, 1);

    EXPECT_GT(tune.period_min_tuned, 0);
    EXPECT_LE(tune.period_min_tuned, 65535);
}


// --- число пропущенных периодов не влияет на поправку (#345, #347) ---

TEST(Wakeup, DriftEstimateIsIndependentOfMissedPeriods)
{
    // attiny отстаёт на 10%: реальный сон длиннее заказанного в 1.1 раза.
    // Сколько бы периодов подряд ни прошло без связи, поправка обязана
    // получиться одна и та же — это свойство железа, а не длины обрыва.
    //
    // base_time == now, поэтому до следующей точки расписания ровно период,
    // и результат зависит только от поправки: 60 / 1.1 = 55.
    const uint16_t PER = 60;

    for (uint16_t n = 1; n <= 24; ++n)
    {
        const long slept = 66L * n;
        WakeupTune tune = tune_wakeup(BASE, BASE, BASE - slept * MIN, PER, PER, n);

        EXPECT_EQ(tune.period_min_tuned, 55) << "периодов: " << n;
    }
}

TEST(Wakeup, DriftEstimateBreaksWithoutPeriodCount)
{
    // Тот же обрыв, но число периодов не передали (как считалось до #357):
    // 1.1 превращается в 13.2, целая часть отбрасывается, и вместо поправки
    // 1.1 получается 1.2 — период уезжает.
    WakeupTune honest = tune_wakeup(BASE, BASE, BASE - 66L * 12 * MIN, 60, 60, 12);
    WakeupTune blind = tune_wakeup(BASE, BASE, BASE - 66L * 12 * MIN, 60, 60, 1);

    EXPECT_EQ(honest.period_min_tuned, 55);
    EXPECT_NE(blind.period_min_tuned, honest.period_min_tuned);
}

TEST(Wakeup, WeeklySyncGivesUsableCorrection)
{
    // Реальный сценарий #357: синхронизация раз в неделю. Период час,
    // attiny спешит на 5% — за 168 пробуждений разница накапливается,
    // и поправка обязана её увидеть.
    const long slept = 57L * 168;   // 57 реальных минут вместо 60 заказанных

    WakeupTune tune = tune_wakeup(BASE, BASE, BASE - slept * MIN, 60, 60, 168);

    // k = 0.95, значит на час реального времени надо заказать ~63 кривых минуты
    EXPECT_EQ(tune.period_min_tuned, 63);
}

TEST(Wakeup, ZeroPeriodsSleptFallsBackToOne)
{
    // Счётчик пробуждений потерялся: считаем, что проспали один период —
    // прежнее поведение, лишь бы не делить на ноль
    WakeupTune zero = tune_wakeup(BASE + 66 * MIN, BASE, BASE, 60, 60, 0);
    WakeupTune one = tune_wakeup(BASE + 66 * MIN, BASE, BASE, 60, 60, 1);

    EXPECT_EQ(zero.period_min_tuned, one.period_min_tuned);
}


// --- целый период с поправкой: для ручного пробуждения (#380) ---

TEST(FullPeriod, EqualsNominalWhenAttinyIsExact)
{
    // k = 1: кривая минута равна реальной
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE - 60 * MIN, 60, 60, 1);

    EXPECT_EQ(tune.period_min_full, 60);
}

TEST(FullPeriod, ShorterWhenAttinyRunsSlow)
{
    // attiny спит на 10% дольше заказанного: чтобы получить час реального
    // времени, заказывать надо 55 кривых минут
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE - 66 * MIN, 60, 60, 1);

    EXPECT_EQ(tune.period_min_full, 55);
}

TEST(FullPeriod, LongerWhenAttinyRunsFast)
{
    // attiny спешит: 57 реальных минут вместо 60 заказанных
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE - 57 * MIN, 60, 60, 1);

    EXPECT_EQ(tune.period_min_full, 63);
}

TEST(FullPeriod, DoesNotDependOnDistanceToNextSlot)
{
    // Главное свойство: period_min_tuned меняется от того, куда целимся,
    // а целый период — только от поправки. Из-за этого нажатие кнопки и
    // промахивалось: в attiny уходило "сколько осталось до точки".
    const time_t SLEPT = BASE - 66 * MIN;

    WakeupTune near_slot = tune_wakeup(BASE, BASE - 1380 * MIN, SLEPT, 1440, 1309, 1);
    WakeupTune far_slot = tune_wakeup(BASE, BASE, SLEPT, 1440, 1309, 1);

    EXPECT_NE(near_slot.period_min_tuned, far_slot.period_min_tuned);
    EXPECT_EQ(near_slot.period_min_full, far_slot.period_min_full);
}

TEST(FullPeriod, DailyPeriodWithDrift)
{
    // Сутки при attiny, спящем на 10% дольше: 1440 / 1.1 = 1309
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE - 1584 * MIN, 1440, 1440, 1);

    EXPECT_EQ(tune.period_min_full, 1309);
}

TEST(FullPeriod, FitsUint16OnLongPeriods)
{
    // 65535 / 0.7 не влезает в uint16_t — как и period_min_tuned,
    // откатываемся к номиналу вместо неопределённого поведения
    const uint16_t PER = 65535;
    WakeupTune tune = tune_wakeup(BASE, BASE, BASE - 45874L * MIN, PER, 65530, 1);

    EXPECT_EQ(tune.period_min_full, PER);
}

TEST(ManualWakeup, UsesMeasuredFullPeriod)
{
    // Нажали кнопку: расписание начинается с этой минуты, значит заказать
    // надо целый период с поправкой
    EXPECT_EQ(period_after_manual_wakeup(1309, 1440), 1309);
}

TEST(ManualWakeup, FallsBackWhenDriftUnknown)
{
    // Поправку ещё не измеряли (новое устройство): заказываем как при смене
    // периода — на 10% меньше номинала. Проснуться раньше цели не страшно,
    // поправка доберёт на следующем цикле; проснуться сильно позже — хуже,
    // ближайшая точка расписания будет уже пропущена.
    EXPECT_EQ(period_after_manual_wakeup(0, 1440), period_after_user_change(1440));
}
