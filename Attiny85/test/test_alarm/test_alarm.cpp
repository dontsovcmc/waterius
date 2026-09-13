#include <gtest/gtest.h>
#include "alarm.h"

/*
Детекция аварий по импульсам счётчика (issue #202).

Время идёт тиками сторожевого таймера по 250 мс - это единственная мера
времени у attiny, и детектор больше ничего не знает. Минуты в тестах нужны
только для читаемости сценариев.

Два критерия, и оба абсолютные:

  ALARM_LEAK - за leak_quanta квантов подряд не встретилось ни одного кванта
               без импульсов. Квант задаёт ЕСП: квант = вес импульса / Q, то
               есть "какой расход считать остановкой воды".
  ALARM_FLOW - в скользящем 30-минутном окне набралось vol_pulses импульсов.

Ни одна тревога не снимается сама. Снимает её человек - reset() по маске.
*/

static const uint16_t TPM = ALARM_TICKS_PER_MINUTE;

// Квант в час при весе 10 л/имп и пороге 10 л/ч: 14400 тиков
static const uint16_t HOUR = 60 * TPM;

/*
Прокрутить n минут без импульсов.
*/
static void idle(AlarmDetector &a, const uint16_t minutes)
{
    for (uint16_t m = 0; m < minutes; m++)
        for (uint16_t t = 0; t < TPM; t++)
            a.on_tick();
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
static void flow(AlarmDetector &a, const uint16_t count, const uint16_t gap)
{
    for (uint16_t i = 0; i < count; i++)
        pulse_after(a, gap);
}

// Протечка: квант час, тревога через 24 занятых кванта
static void setup_leak(AlarmDetector &a, const uint16_t quanta = 24)
{
    a.configure(HOUR, quanta, 0);
}

/* ---------------- протечка ---------------- */

TEST(Leak, DrippingTapIsCaught)
{
    // Капающий кран: импульс раз в 10 минут. Каждый час занят
    AlarmDetector a;
    setup_leak(a, 3);

    flow(a, 3 * 6, 10);

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Leak, SlowFlowNearTheLimitIsCaught)
{
    // Импульс раз в 50 минут - это 12 л/ч при весе 10. Медленнее кванта не
    // бывает: расход ровно в Q даёт импульс ровно за квант
    AlarmDetector a;
    setup_leak(a, 3);

    flow(a, 4, 50);

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Leak, SlowerThanQuantumBlursTheCount)
{
    // Импульс раз в 70 минут - чуть реже кванта. Пустые кванты появляются, но
    // не каждый раз: течь ниже порога не исчезает, а становится ненадёжной.
    // Порог в л/ч - это граница гарантии, а не обрыв
    AlarmDetector a;
    setup_leak(a, 3);

    flow(a, 10, 70);

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Leak, TwiceTheQuantumIsInvisible)
{
    // Зазор вдвое длиннее кванта: пустой квант гарантирован между любыми
    // двумя импульсами, счёт обнуляется, и такая течь не ловится никогда
    AlarmDetector a;
    setup_leak(a, 3);

    flow(a, 20, 130);

    EXPECT_FALSE(a.state & ALARM_LEAK);
}

TEST(Leak, QuietQuantumRestartsCount)
{
    // Обычный быт: расход, тишина, снова расход
    AlarmDetector a;
    setup_leak(a, 3);

    setup_leak(a, 5);  // пяти занятых квантов подряд не наберётся

    flow(a, 12, 10);   // два часа расхода - три кванта
    idle(a, 130);      // больше двух пустых квантов: счёт с нуля
    flow(a, 12, 10);   // ещё два часа

    EXPECT_FALSE(a.state & ALARM_LEAK);
}

TEST(Leak, SinglePulseAfterLongSilenceIsNotLeak)
{
    // Регресс #405: одиночный импульс не должен запоминаться как расход
    AlarmDetector a;
    setup_leak(a, 3);

    idle(a, 600);
    a.on_pulse();
    idle(a, 600);

    EXPECT_FALSE(a.state & ALARM_LEAK);
}

TEST(Leak, RoundTheClockConsumptionRaisesAlarm)
{
    // Много жильцов, воду берут круглые сутки: тихого часа нет, и тревога
    // поднимется. Так и задумано - таким домам эта тревога не подходит, и
    // они её не включат
    AlarmDetector a;
    setup_leak(a, 24);

    flow(a, 24 * 2, 30);

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Leak, DisabledThresholdNeverFires)
{
    AlarmDetector a;
    a.configure(HOUR, 0, 0); // квант есть, порога нет

    flow(a, 200, 10);

    EXPECT_FALSE(a.state & ALARM_LEAK);
}

TEST(Leak, NoQuantumNoAlarm)
{
    AlarmDetector a;
    a.configure(0, 24, 0); // кванта нет - считать нечего

    flow(a, 200, 10);

    EXPECT_FALSE(a.state & ALARM_LEAK);
}

/* ---------------- снятия по времени нет ---------------- */

TEST(Leak, StoppedFlowDoesNotClearAlarm)
{
    // Главное свойство новой модели: вода кончилась, а тревога висит
    AlarmDetector a;
    setup_leak(a, 3);

    flow(a, 18, 10);
    ASSERT_TRUE(a.state & ALARM_LEAK);

    idle(a, 24 * 60); // сутки тишины

    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Reset, ClearsAndRestartsCount)
{
    AlarmDetector a;
    setup_leak(a, 3);

    flow(a, 18, 10);
    ASSERT_TRUE(a.state & ALARM_LEAK);

    a.reset(ALARM_LEAK);
    EXPECT_FALSE(a.state & ALARM_LEAK);

    // Счёт начался заново: двух занятых квантов на повтор не хватит
    flow(a, 12, 10);
    EXPECT_FALSE(a.state & ALARM_LEAK);

    flow(a, 6, 10);
    EXPECT_TRUE(a.state & ALARM_LEAK);
}

TEST(Reset, IsNews)
{
    AlarmDetector a;
    setup_leak(a, 3);

    flow(a, 18, 10);
    a.changed = 0;

    a.reset(ALARM_LEAK);

    EXPECT_TRUE(a.changed);
}

TEST(Reset, WithoutAlarmIsNotNews)
{
    AlarmDetector a;
    setup_leak(a, 3);
    a.changed = 0;

    a.reset(ALARM_MASK);

    EXPECT_FALSE(a.changed);
}

TEST(Reset, TouchesOnlyMaskedAlarms)
{
    AlarmDetector a;
    a.configure(HOUR, 3, 3);

    flow(a, 3, 1);             // три импульса в одном окне - большой расход
    ASSERT_TRUE(a.state & ALARM_FLOW);
    flow(a, 18, 10);
    ASSERT_TRUE(a.state & ALARM_LEAK);

    a.reset(ALARM_LEAK);

    EXPECT_FALSE(a.state & ALARM_LEAK);
    EXPECT_TRUE(a.state & ALARM_FLOW);
}

/* ---------------- много воды сразу ---------------- */

TEST(Flow, VolumeInWindowRaisesAlarm)
{
    AlarmDetector a;
    a.configure(0, 0, 30); // 30 импульсов за полчаса = 300 л при весе 10

    flow(a, 30, 1);

    EXPECT_TRUE(a.state & ALARM_FLOW);
}

TEST(Flow, SameVolumeStretchedIsNotAlarm)
{
    // Тот же объём, но за три часа - это не прорыв
    AlarmDetector a;
    a.configure(0, 0, 30);

    flow(a, 30, 6);

    EXPECT_FALSE(a.state & ALARM_FLOW);
}

TEST(Flow, WindowSlides)
{
    // Окно скользящее: порог набирается в любые полчаса, а не только между
    // круглыми получасами
    AlarmDetector a;
    a.configure(0, 0, 10);

    flow(a, 6, 4);   // 24 минуты, 6 импульсов
    idle(a, 2);
    flow(a, 4, 1);   // ещё 4 - десять штук уложились в 30 минут

    EXPECT_TRUE(a.state & ALARM_FLOW);
}

TEST(Flow, OldPulsesLeaveTheWindow)
{
    AlarmDetector a;
    a.configure(0, 0, 10);

    flow(a, 9, 1);
    idle(a, 60);     // старые импульсы вышли из окна
    flow(a, 9, 1);

    EXPECT_FALSE(a.state & ALARM_FLOW);
}

TEST(Flow, VacationAlarmsOnFirstPulse)
{
    // Режим "я уехал": ЕСП шлёт порог в один импульс
    AlarmDetector a;
    a.configure(0, 0, 1);

    a.on_pulse();

    EXPECT_TRUE(a.state & ALARM_FLOW);
}

TEST(Flow, DisabledThresholdNeverFires)
{
    AlarmDetector a;
    a.configure(0, 0, 0);

    flow(a, 200, 1);

    EXPECT_FALSE(a.state & ALARM_FLOW);
}

TEST(Flow, StoppedFlowDoesNotClearAlarm)
{
    AlarmDetector a;
    a.configure(0, 0, 10);

    flow(a, 10, 1);
    ASSERT_TRUE(a.state & ALARM_FLOW);

    idle(a, 24 * 60);

    EXPECT_TRUE(a.state & ALARM_FLOW);
}

/* ---------------- датчик протечки ---------------- */

TEST(Wet, ClosedRaises)
{
    AlarmDetector a;
    a.set_wet(true);
    EXPECT_TRUE(a.state & ALARM_WET);
}

TEST(Wet, OpenedDoesNotClear)
{
    // Дребезг перестаёт существовать как явление: намок - высох - намок
    // даёт одну тревогу и один сеанс
    AlarmDetector a;
    a.set_wet(true);
    a.changed = 0;

    for (int i = 0; i < 100; i++)
    {
        a.set_wet(false);
        a.set_wet(true);
    }

    EXPECT_TRUE(a.state & ALARM_WET);
    EXPECT_FALSE(a.changed);
}

TEST(Wet, ResetClears)
{
    AlarmDetector a;
    a.set_wet(true);

    a.reset(ALARM_WET);

    EXPECT_FALSE(a.state & ALARM_WET);
}

/* ---------------- новость о тревоге ---------------- */

TEST(Changed, RaiseIsNewsOnce)
{
    AlarmDetector a;
    a.configure(0, 0, 1);

    a.on_pulse();
    EXPECT_TRUE(a.changed);

    a.changed = 0;
    a.on_pulse();
    EXPECT_FALSE(a.changed); // состояние не сменилось - докладывать нечего
}

TEST(Report, TriesAreBounded)
{
    AlarmReport r;
    r.start();

    int tries = 0;
    while (r.failed())
        tries++;

    EXPECT_EQ(tries, ALARM_MAX_TRIES - 1);
    EXPECT_FALSE(r.pending);
}

TEST(Report, ConfirmStopsRetries)
{
    AlarmReport r;
    r.start();
    r.confirm();

    EXPECT_FALSE(r.pending);
    EXPECT_FALSE(r.failed());
}

TEST(Report, NewAlarmResetsTries)
{
    AlarmReport r;
    r.start();
    r.failed();
    r.failed();

    r.start();

    EXPECT_EQ(r.tries, 0);
    EXPECT_TRUE(r.pending);
}
