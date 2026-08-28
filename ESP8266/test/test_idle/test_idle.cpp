#include <gtest/gtest.h>
#include "core/idle.h"

/*
Режим "выходить на связь только при расходе воды" (#361).

Период пробуждения остаётся тем, что задал пользователь: устройство исправно
просыпается и читает attiny, поэтому расход замечается ровно за период, а не
"когда-нибудь". Экономится сеанс Wi-Fi — единственная по-настоящему дорогая
часть цикла.

Отсюда и границы тестов: проверяем решение "включать ли радио", а не длину
сна. Длину сна этот режим не трогает вовсе.
*/

namespace
{
    const uint16_t FIVE = 5;            // кейс из issue: наблюдение почти в реальном времени
    const uint16_t QUARTER = 15;        // типичный период для Home Assistant
    const uint16_t DAILY = 1440;        // период по умолчанию
    const bool ON = true;
    const bool OFF = false;
    const bool CONSUMED = true;
    const bool SILENCE = false;
}

// --- обнаружение расхода ---

TEST(ConsumptionDetected, SameImpulsesMeanSilence)
{
    EXPECT_FALSE(consumption_detected(100, 100, 200, 200));
}

TEST(ConsumptionDetected, EitherChannelCounts)
{
    EXPECT_TRUE(consumption_detected(101, 100, 200, 200));
    EXPECT_TRUE(consumption_detected(100, 100, 201, 200));
}

TEST(ConsumptionDetected, CounterRollbackWakesDevice)
{
    // attiny обнулился: замена батареек или сброс. Ровно тот случай, когда
    // выходить на связь надо, а не молчать трое суток
    EXPECT_TRUE(consumption_detected(0, 1000, 0, 2000));
}

TEST(ConsumptionDetected, FirstCycleAfterSetupCounts)
{
    // impulses_previous == 0 сразу после настройки: накопленный объём
    // считается расходом, и первый выход на связь случится как обычно
    EXPECT_TRUE(consumption_detected(7, 0, 0, 0));
}

// --- решение выходить на связь ---

TEST(NeedTransmit, ModeOffChangesNothing)
{
    // Выключенный режим обязан вести себя ровно как прошивка без него
    EXPECT_TRUE(need_transmit(OFF, SILENCE, 0));
    EXPECT_TRUE(need_transmit(OFF, SILENCE, MAX_SILENCE_MIN - 1));
}

TEST(NeedTransmit, ConsumptionAlwaysGoesOnAir)
{
    EXPECT_TRUE(need_transmit(ON, CONSUMED, 0));
    EXPECT_TRUE(need_transmit(ON, CONSUMED, 5));
}

TEST(NeedTransmit, SilenceKeepsRadioOff)
{
    EXPECT_FALSE(need_transmit(ON, SILENCE, 0));
    EXPECT_FALSE(need_transmit(ON, SILENCE, MAX_SILENCE_MIN - 1));
}

TEST(NeedTransmit, ThreeDaysOfSilenceStillReports)
{
    // Отметка "устройство живо". В Zigbee и Matter это maximum reporting
    // interval, и без неё облако сочтёт устройство пропавшим.
    //
    // Трое суток, а не больше: облако шлёт письмо после max(5 суток,
    // period_min * 2 + 2 часа). Обычная тишина в этот порог укладывается, а
    // вот пропущенная отметка — уже нет, и письмо в этом случае уместно:
    // устройство дважды не смогло достучаться до сервера.
    EXPECT_EQ(MAX_SILENCE_MIN, 4320u);
    EXPECT_LT(MAX_SILENCE_MIN, 5u * 24 * 60);

    EXPECT_TRUE(need_transmit(ON, SILENCE, MAX_SILENCE_MIN));
    EXPECT_TRUE(need_transmit(ON, SILENCE, MAX_SILENCE_MIN + 100));
}

TEST(NeedTransmit, HowManyWakeupsAreSkipped)
{
    // Пятиминутный период, сутки без расхода: 288 пробуждений, и ни одного
    // сеанса Wi-Fi. Ради этого всё и затевалось
    uint16_t silence = 0;
    int transmits = 0;

    for (int wakeup = 0; wakeup < 288; ++wakeup)
    {
        silence = add_minutes(silence, FIVE);
        if (need_transmit(ON, SILENCE, silence))
        {
            transmits++;
            silence = 0;
        }
    }

    EXPECT_EQ(transmits, 0);
    EXPECT_EQ(silence, 288 * FIVE);
}

TEST(NeedTransmit, HeartbeatFiresOnceInThreeDays)
{
    // Неделя тишины на пятиминутном периоде: ровно два выхода на связь
    uint16_t silence = 0;
    int transmits = 0;

    for (int wakeup = 0; wakeup < 7 * 24 * 12; ++wakeup)
    {
        silence = add_minutes(silence, FIVE);
        if (need_transmit(ON, SILENCE, silence))
        {
            transmits++;
            silence = 0;
        }
    }

    EXPECT_EQ(transmits, 2);
}

TEST(NeedTransmit, LongUserPeriodIsNotShortened)
{
    // Суточный период: срок молчания наступит на третьи сутки, то есть
    // режим просто не мешает — устройство и так выходит на связь чаще
    uint16_t silence = add_minutes(0, DAILY);

    EXPECT_FALSE(need_transmit(ON, SILENCE, silence));
    EXPECT_TRUE(need_transmit(ON, SILENCE, add_minutes(silence, 2 * DAILY)));
}

// --- счётчик молчания ---

TEST(AddMinutes, Accumulates)
{
    EXPECT_EQ(add_minutes(0, QUARTER), 15);
    EXPECT_EQ(add_minutes(4305, QUARTER), 4320);
}

TEST(AddMinutes, SaturatesInsteadOfWrapping)
{
    // Переполнение отодвинуло бы отметку "я жив" на новый круг, и устройство
    // молчало бы дольше, чем разрешено
    EXPECT_EQ(add_minutes(65535, 1), 65535);
    EXPECT_EQ(add_minutes(65530, 100), 65535);
    EXPECT_EQ(add_minutes(60000, 60000), 65535);
}

TEST(AddMinutes, SaturatedCounterStillDemandsTransmission)
{
    // Насыщение не должно превращаться в "срок никогда не наступит"
    EXPECT_TRUE(need_transmit(ON, SILENCE, 65535));
}

// --- остановка потребления (#202) ---

TEST(ConsumptionStopped, DisabledStaysSilent)
{
    // Порог 0 - тревога выключена, сколько бы ни молчал счётчик
    EXPECT_FALSE(consumption_stopped(0, 0));
    EXPECT_FALSE(consumption_stopped(65535, 0));
}

TEST(ConsumptionStopped, RaisesAtThreshold)
{
    EXPECT_FALSE(consumption_stopped(48 * 60 - 1, 48));
    EXPECT_TRUE(consumption_stopped(48 * 60, 48));
    EXPECT_TRUE(consumption_stopped(48 * 60 + 1, 48));
}

TEST(ConsumptionStopped, ThresholdDoesNotOverflow)
{
    // Час, умноженный на 60, из uint16_t выходит уже на 1093-м часе:
    // в 16 битах порог свернулся бы и тревога сработала бы почти сразу
    EXPECT_FALSE(consumption_stopped(60, ALARM_STOP_MAX_HOURS));
    EXPECT_TRUE(consumption_stopped(65520, ALARM_STOP_MAX_HOURS));
}

TEST(IdleMinutes, ResetsOnConsumption)
{
    EXPECT_EQ(update_idle_minutes(true, 5000, 15), 0);
}

TEST(IdleMinutes, AccumulatesAcrossWakeups)
{
    uint16_t idle = 0;

    for (int i = 0; i < 4; i++)
        idle = update_idle_minutes(false, idle, 15);

    EXPECT_EQ(idle, 60);
}

TEST(IdleMinutes, Saturates)
{
    // 45 суток тишины - дальше точное значение уже ничего не решает,
    // но свернуться счётчик не должен: тревога снялась бы сама собой
    EXPECT_EQ(update_idle_minutes(false, 65535, 1440), 65535);
    EXPECT_TRUE(consumption_stopped(update_idle_minutes(false, 65535, 1440),
                                    ALARM_STOP_MAX_HOURS));
}

TEST(ConsumptionStopped, OnlyForInputsThatCount)
{
    // Вес импульса не нужен: сравнивается сам факт прироста, поэтому остановку
    // можно задать и до того, как вход настроен до конца
    EXPECT_TRUE(counts_impulses(CounterType::NAMUR));
    EXPECT_TRUE(counts_impulses(CounterType::ELECTRONIC));

    EXPECT_FALSE(counts_impulses(CounterType::NONE));
    EXPECT_FALSE(counts_impulses(CounterType::LEAKAGE));
    EXPECT_FALSE(counts_impulses(CounterType::LEAKAGE_NC));
}
