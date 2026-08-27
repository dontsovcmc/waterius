#include <gtest/gtest.h>
#include "alarm.h"

/*
Детекция аварий по импульсам счётчика (issue #202).

Время здесь идёт тиками сторожевого таймера по 250 мс; минуту отсчитывает
главный цикл attiny одним счётчиком на оба канала, поэтому в тестах минуты
подаются явно — helper run().

Разбор режимов и сценариев — docs/alarms.md.
*/

// Тиков в минуте: столько раз attiny просыпается по watchdog
static const uint16_t TPM = ALARM_TICKS_PER_MINUTE;

/*
Прокрутить n минут без импульсов.
*/
static void idle(AlarmDetector &a, const uint16_t minutes)
{
    for (uint16_t m = 0; m < minutes; m++)
    {
        for (uint16_t t = 0; t < TPM; t++)
            a.on_tick();
        a.on_minute();
    }
}

/*
Прокрутить n минут и выдать импульс в конце.
*/
static void pulse_after(AlarmDetector &a, const uint16_t minutes)
{
    idle(a, minutes);
    a.on_pulse();
}

/*
Расход с постоянным ритмом: count импульсов через каждые gap минут.
*/
static void steady(AlarmDetector &a, const uint16_t gap, const uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
        pulse_after(a, gap);
}

TEST(Alarm, DisabledStaysSilent)
{
    // Пороги не заданы — что бы ни творилось на входе, тревог нет
    AlarmDetector a;

    for (int i = 0; i < 50; i++)
    {
        a.on_tick();
        a.on_pulse();
    }
    idle(a, 300);

    EXPECT_EQ(a.state, 0);
    EXPECT_EQ(a.changed, 0);
}

TEST(Alarm, FastIntervalRaisesFlow)
{
    // Порог — импульс не чаще чем раз в 4 минуты
    AlarmDetector a;
    a.configure(4 * TPM, 0);

    pulse_after(a, 10); // первый импульс, интервала ещё нет
    pulse_after(a, 2);  // пришёл вдвое быстрее порога

    EXPECT_TRUE(a.state & ALARM_FLOW);
    EXPECT_EQ(a.changed, 1);
}

TEST(Alarm, SlowIntervalKeepsSilent)
{
    // Ровно на пороге — это ещё не превышение
    AlarmDetector a;
    a.configure(4 * TPM, 0);

    pulse_after(a, 10);
    pulse_after(a, 4);

    EXPECT_FALSE(a.state & ALARM_FLOW);
}

TEST(Alarm, FlowClearsAtHalfThreshold)
{
    /*
    В ветке alarm тревога залипала навсегда: alarm присваивался и нигде не
    возвращался в NORM, второй аварии не было бы уже никогда.
    */
    AlarmDetector a;
    a.configure(4 * TPM, 0);

    pulse_after(a, 10);
    pulse_after(a, 2);
    ASSERT_TRUE(a.state & ALARM_FLOW);

    // Пауза ровно в двойной порог тревогу ещё держит
    idle(a, 8);
    EXPECT_TRUE(a.state & ALARM_FLOW);

    // Дальше расход упал ниже половины порога
    idle(a, 1);
    EXPECT_FALSE(a.state & ALARM_FLOW);
}

TEST(Alarm, DrippingTapTwoHours)
{
    /*
    Главный сценарий issue #202: кран капает и равномерно расходует воду два
    часа. Вес 10 л/имп, расход 60 л/ч — импульс раз в 10 минут, порог 120 мин.
    */
    AlarmDetector a;
    a.configure(0, 120);

    pulse_after(a, 10); // 0:10 ритма ещё нет
    pulse_after(a, 10); // 0:20 интервал запомнен, счётчик пошёл

    // 0:20 -> 2:10, ещё 11 импульсов через 10 минут
    for (int i = 0; i < 11; i++)
    {
        pulse_after(a, 10);
    }

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Alarm, SlowLeakBeatsFixedWindow)
{
    /*
    Импульс раз в 30 минут. Окно фиксированной длины (5 или даже 30 минут) из
    ветки alarm такую протечку теряет: окна оказываются пустыми. Ритм — нет.
    */
    AlarmDetector a;
    a.configure(0, 120);

    steady(a, 30, 8); // 4 часа расхода

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Alarm, NormalUsageNoAlarm)
{
    /*
    Обычный быт: душ, потом тишина три часа, потом посудомойка. Пауза длиннее
    двойного интервала — ритм сбит, тревоги нет.
    */
    AlarmDetector a;
    a.configure(0, 120);

    steady(a, 1, 20); // душ, импульс раз в минуту
    idle(a, 180);     // тишина
    steady(a, 1, 15); // посудомойка

    EXPECT_FALSE(a.state & ALARM_LEAK);
}

TEST(Alarm, JitterWithinTwiceIsStillLeak)
{
    // Интервалы гуляют, но каждый укладывается в двойной предыдущий
    AlarmDetector a;
    a.configure(0, 60);

    const uint16_t gaps[] = {10, 18, 11, 20, 12, 10, 15, 9};
    for (int round = 0; round < 3; round++)
    {
        for (uint16_t g : gaps)
            pulse_after(a, g);
    }

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Alarm, PauseOverTwiceResets)
{
    AlarmDetector a;
    a.configure(0, 60);

    steady(a, 10, 10);
    ASSERT_TRUE(a.state & ALARM_LEAK);

    // Кран закрыли: первая же пауза длиннее двойного интервала гасит тревогу
    idle(a, 21);

    EXPECT_FALSE(a.state & ALARM_LEAK);
    EXPECT_EQ(a.run_min, 0);
}

TEST(Alarm, TwelveHoursOfLeakFits)
{
    // anklimov настраивает 8-12 часов: 720 минут не должны переполнить счётчик
    AlarmDetector a;
    a.configure(0, 600);

    steady(a, 10, 74); // 740 минут непрерывного расхода

    EXPECT_TRUE(a.state & ALARM_LEAK);
    EXPECT_GT(a.run_min, 600);
}

TEST(Alarm, RhythmStartsFromSecondImpulse)
{
    /*
    До второго импульса измерять нечего: интервала нет, время непрерывного
    расхода не копится. Со второго — ритм известен, счётчик идёт.
    */
    AlarmDetector a;
    a.configure(0, 2);

    a.on_pulse(); // первый импульс: интервала ещё нет
    idle(a, 3);
    EXPECT_EQ(a.run_min, 0);
    EXPECT_FALSE(a.state & ALARM_LEAK);

    pulse_after(a, 5); // второй импульс: интервал 5 минут запомнен
    idle(a, 3);
    EXPECT_GT(a.run_min, 0);
    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Alarm, WetAndFlowCoexist)
{
    /*
    В ветке alarm состояние было перечислением, а не маской: датчик протечки
    затирал бы уже поднятую тревогу о прорыве.
    */
    AlarmDetector a;
    a.configure(4 * TPM, 0);

    pulse_after(a, 10);
    pulse_after(a, 1);
    ASSERT_TRUE(a.state & ALARM_FLOW);

    a.set_wet(true);

    EXPECT_TRUE(a.state & ALARM_FLOW);
    EXPECT_TRUE(a.state & ALARM_WET);
}

TEST(Alarm, WetSensorRaisesAndClears)
{
    AlarmDetector a;

    a.set_wet(true);
    EXPECT_TRUE(a.state & ALARM_WET);
    EXPECT_EQ(a.changed, 1);

    a.changed = 0;
    a.set_wet(false);
    EXPECT_FALSE(a.state & ALARM_WET);
    EXPECT_EQ(a.changed, 1);
}

TEST(Alarm, ChangedOnlyOnTransition)
{
    // Повторное срабатывание того же условия новостью не является
    AlarmDetector a;

    a.set_wet(true);
    a.changed = 0;

    a.set_wet(true);
    a.set_wet(true);

    EXPECT_EQ(a.changed, 0);
}

TEST(Alarm, StateHeldUntilConfirmed)
{
    /*
    Пока доклад не подтверждён, состояние не снимается: иначе тревога успевала
    бы погаснуть до того, как ЕСП прочитает байт флагов, и в отчёт ушло бы
    "всё хорошо".
    */
    AlarmDetector a;

    a.set_wet(true);
    a.hold(true);
    a.set_wet(false);
    EXPECT_TRUE(a.state & ALARM_WET);

    a.hold(false);
    a.set_wet(false);
    EXPECT_FALSE(a.state & ALARM_WET);
}

TEST(Alarm, TicksSaturate)
{
    // Сутки без импульсов не должны переполнить счётчик тиков
    AlarmDetector a;
    a.configure(4 * TPM, 0);

    idle(a, 24 * 60);

    EXPECT_EQ(a.ticks, UINT16_MAX);
    EXPECT_EQ(a.state, 0);
}

/*
Доставка доклада. В ветке alarm признак гасился сразу после сеанса, даже если
ЕСП не достучалась до сервера, — авария молча пропадала.
*/

TEST(AlarmReportTest, RetriesUntilConfirmed)
{
    AlarmReport r;
    r.start();

    int wakeups = 1; // первый сеанс уже случился
    while (r.failed())
        wakeups++;

    EXPECT_EQ(wakeups, ALARM_MAX_TRIES);
    EXPECT_EQ(r.pending, 0);
}

TEST(AlarmReportTest, ConfirmStopsRetries)
{
    AlarmReport r;
    r.start();

    ASSERT_TRUE(r.failed()); // первый сеанс не дошёл
    r.confirm();             // второй дошёл

    EXPECT_EQ(r.pending, 0);
    EXPECT_EQ(r.tries, 0);
    EXPECT_FALSE(r.failed());
}

/*
Полный цикл датчика протечки: намок - доложили, высох - доложили снова.

Ниже session() повторяет то, что делает главный цикл attiny вокруг сеанса:
новость превращается в доклад, состояние замораживается до подтверждения,
бюджет внеплановых сеансов тратится или начинается заново.

@param scheduled сеанс по расписанию, а не по тревоге
@return состоялся ли внеплановый сеанс
*/
static bool session(AlarmDetector &a, AlarmReport &r, const bool delivered,
                    const bool scheduled = false)
{
    const bool alarm_wake = r.budget_left() && (r.pending || a.changed);

    if (!scheduled && !alarm_wake)
        return false; // будить нечем: ни новости, ни бюджета

    if (a.changed)
    {
        r.start();
        a.changed = 0;
    }
    a.hold(r.pending);

    if (delivered)
        r.confirm();
    else if (r.pending)
        r.failed();

    if (scheduled)
        r.new_period();
    else
        r.spend();

    a.hold(r.pending && r.budget_left());
    return !scheduled && alarm_wake;
}

/*
Сколько внеплановых сеансов вытянет из устройства дребезжащий датчик.
*/
static int chatter(AlarmDetector &a, AlarmReport &r, const int transitions)
{
    int wakeups = 0;

    for (int i = 0; i < transitions; i++)
    {
        a.set_wet(i % 2 == 0);
        if (session(a, r, true))
            wakeups++;
    }
    return wakeups;
}

TEST(AlarmReportTest, WetSensorReportsBothEdges)
{
    AlarmDetector a;
    AlarmReport r;

    // Намок
    a.set_wet(true);
    EXPECT_TRUE(session(a, r, true));
    EXPECT_TRUE(a.state & ALARM_WET);

    // Высох - это тоже новость, и её тоже надо отвезти
    a.set_wet(false);
    EXPECT_EQ(a.changed, 1);
    EXPECT_TRUE(session(a, r, true));
    EXPECT_FALSE(a.state & ALARM_WET);

    // Больше докладывать нечего
    EXPECT_FALSE(session(a, r, true));
}

TEST(AlarmReportTest, ScheduledSessionCarriesTheNews)
{
    /*
    Плановый сеанс увозит состояние не хуже внепланового: Header ЕСП читает
    всегда. Раньше флаг "есть новость" после него оставался, и устройство
    просыпалось ещё раз - доложить уже доложенное.
    */
    AlarmDetector a;
    AlarmReport r;

    a.set_wet(true);
    session(a, r, true, true); // сеанс по расписанию, подвернулся первым

    EXPECT_EQ(a.changed, 0);
    EXPECT_EQ(r.pending, 0);
    EXPECT_FALSE(session(a, r, true)); // второго пробуждения быть не должно
}

TEST(AlarmReportTest, StateFrozenBetweenRetries)
{
    /*
    Между попытками доставки состояние держится: иначе тревога успела бы
    погаснуть, и повторный сеанс отвёз бы на сервер "всё хорошо" - то есть
    авария так и осталась бы незамеченной.
    */
    AlarmDetector a;
    AlarmReport r;

    a.set_wet(true);
    r.start();
    a.hold(r.pending);

    // Первый сеанс не дошёл - датчик успел высохнуть
    ASSERT_TRUE(r.failed());
    a.hold(r.pending);
    a.set_wet(false);
    EXPECT_TRUE(a.state & ALARM_WET);

    // Доставили - держать больше нечего
    r.confirm();
    a.hold(r.pending);
    a.set_wet(false);
    EXPECT_FALSE(a.state & ALARM_WET);
}

TEST(AlarmReportTest, NewAlarmResetsTries)
{
    AlarmReport r;
    r.start();

    ASSERT_TRUE(r.failed());
    ASSERT_TRUE(r.failed());

    r.start(); // пришла новая тревога — счёт попыток заново

    int wakeups = 1;
    while (r.failed())
        wakeups++;

    EXPECT_EQ(wakeups, ALARM_MAX_TRIES);
}


/*
Дребезг датчика протечки ограничен бюджетом, а не только паузой (issue #202).

Пауза в ALARM_HOLD_MIN задаёт темп - двенадцать сеансов в час, - но не потолок:
ALARM_MAX_TRIES считает попытки одного доклада, а каждый новый переход датчика
начинал счёт заново. Без бюджета связка "намок - высох - намок" держала бы
устройство в эфире до конца батареи.
*/
TEST(AlarmReportTest, WetChatterIsBounded)
{
    AlarmDetector a;
    AlarmReport r;

    EXPECT_EQ(chatter(a, r, 20), ALARM_MAX_SESSIONS);
}

/*
Плановый сеанс увозит текущее состояние, поэтому бюджет начинается заново.
*/
TEST(AlarmReportTest, ScheduledSessionRestoresBudget)
{
    AlarmDetector a;
    AlarmReport r;

    ASSERT_EQ(chatter(a, r, 20), ALARM_MAX_SESSIONS);
    ASSERT_FALSE(r.budget_left());

    session(a, r, true, true);
    EXPECT_TRUE(r.budget_left());

    a.set_wet(true);
    EXPECT_TRUE(session(a, r, true));
}

/*
Исчерпанный бюджет новость не теряет: она уезжает ближайшим плановым сеансом.
*/
TEST(AlarmReportTest, ExhaustedBudgetKeepsTheNews)
{
    AlarmDetector a;
    AlarmReport r;

    ASSERT_EQ(chatter(a, r, 20), ALARM_MAX_SESSIONS);

    a.set_wet(true);
    ASSERT_EQ(a.changed, 1);

    EXPECT_FALSE(session(a, r, true)); // внепланово больше не будим
    EXPECT_EQ(a.changed, 1);           // но и новость не выбрасываем

    session(a, r, true, true);
    EXPECT_EQ(a.changed, 0);
}

/*
Кончился бюджет - отпускаем состояние.

Держать его дальше значит увезти плановым сеансом застывшую тревогу вместо
правды: за сутки до этого сеанса датчик успеет высохнуть.
*/
TEST(AlarmReportTest, ExhaustedBudgetReleasesHold)
{
    AlarmDetector a;
    AlarmReport r;

    // Четыре новости доехали
    ASSERT_EQ(chatter(a, r, 4), 4);

    // Пятая - уже без сети: бюджет кончился, а доклад не подтверждён
    a.set_wet(true);
    ASSERT_TRUE(session(a, r, false));
    ASSERT_EQ(r.pending, 1);
    ASSERT_FALSE(r.budget_left());

    a.set_wet(false);
    EXPECT_FALSE(a.state & ALARM_WET);
}
