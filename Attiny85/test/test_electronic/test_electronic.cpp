#include <gtest/gtest.h>
#include "electronic.h"

/*
Подсчёт импульсов электронного выхода (issue #379).

Импульс газового счётчика Гранд длится 0,7–1,5 мс. Главный цикл attiny за это
время не успевает: он просыпается из сна, стоит в очереди за записью EEPROM и
обменом с ЕСП. Поэтому решение «был импульс» принимается по фронту в
прерывании, а не по уровню, прочитанному позже.

Ниже FRONT — это вызов из ISR с уровнем пина, TAKE — забор результата главным
циклом. Тик опроса (250 мс) отмечен как time_event.
*/

static const bool TIME_TICK = true;
static const bool NO_TICK = false;

static const bool HIGH_LEVEL = true;
static const bool LOW_LEVEL = false;

/*
Копировать ElectronicInput нельзя: volatile-поле удаляет конструктор копии.
Поэтому вход настраивается на месте, а не возвращается фабрикой.

ACTIVE_LOW — выход с открытым стоком: покой высокий, импульс замыкает на минус.
ACTIVE_HIGH — ELECTRONIC_HIGH: счётчик сам выдаёт положительный импульс.
*/
#define ACTIVE_LOW(name)  ElectronicInput name; name.enable(false)
#define ACTIVE_HIGH(name) ElectronicInput name; name.enable(true)

TEST(Electronic, ShortPulseIsNotLost)
{
    /*
    Суть #379: импульс кончился раньше, чем главный цикл проснулся. Оба
    фронта пришли до take(), и уровень к этому моменту снова в покое —
    импульс всё равно обязан быть засчитан.
    */
    ACTIVE_LOW(input);

    input.on_front(LOW_LEVEL);   // начало импульса
    input.on_front(HIGH_LEVEL);  // конец импульса, 0,7 мс спустя

    EXPECT_TRUE(input.take(NO_TICK));
}

TEST(Electronic, PulseCountedOnceUntilIdle)
{
    // Дребезг внутри импульса не должен превращаться в несколько импульсов
    ACTIVE_LOW(input);

    input.on_front(LOW_LEVEL);
    input.on_front(LOW_LEVEL);
    input.on_front(LOW_LEVEL);

    EXPECT_TRUE(input.take(NO_TICK));
    EXPECT_FALSE(input.take(NO_TICK));
}

TEST(Electronic, ActiveLowCountsFallingEdge)
{
    ACTIVE_LOW(input);

    input.on_front(HIGH_LEVEL);  // покой, импульса нет
    EXPECT_FALSE(input.take(NO_TICK));

    input.on_front(LOW_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));
}

TEST(Electronic, ActiveHighCountsRisingEdge)
{
    ACTIVE_HIGH(input);

    input.on_front(LOW_LEVEL);   // покой, импульса нет
    EXPECT_FALSE(input.take(NO_TICK));

    input.on_front(HIGH_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));
}

TEST(Electronic, SecondPulseCountedAfterReturnToIdle)
{
    ACTIVE_LOW(input);

    input.on_front(LOW_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));

    input.on_front(HIGH_LEVEL);
    EXPECT_FALSE(input.take(NO_TICK));

    input.on_front(LOW_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));
}

TEST(Electronic, ActiveLowHasNoDeadTime)
{
    /*
    Электросчётчик 3200 имп/кВт·ч на 22 кВт - это 20 импульсов в секунду, а тик
    опроса четыре раза в секунду. Каждый импульс считается и без тика между ними.
    */
    ACTIVE_LOW(input);

    for (int i = 0; i < 20; i++)
    {
        input.on_front(LOW_LEVEL);
        EXPECT_TRUE(input.take(NO_TICK)) << "импульс " << i;
        input.on_front(HIGH_LEVEL);
        EXPECT_FALSE(input.take(NO_TICK));
    }
}

TEST(Electronic, DeadTimeDropsFastRepeat)
{
    /*
    Наводка на высокоомном входе выглядит как пачка фронтов. Мёртвое время
    оставляет от неё один импульс, а не десяток.
    */
    ACTIVE_HIGH(input);

    input.on_front(HIGH_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));

    input.on_front(LOW_LEVEL);
    input.on_front(HIGH_LEVEL);
    EXPECT_FALSE(input.take(NO_TICK));

    input.on_front(LOW_LEVEL);
    input.on_front(HIGH_LEVEL);
    EXPECT_FALSE(input.take(NO_TICK));
}

TEST(Electronic, DeadTimeExpiresOnTimeEvent)
{
    // Мёртвое время меряется тиками опроса: один тик — 250 мс
    ACTIVE_HIGH(input);

    input.on_front(HIGH_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));

    EXPECT_FALSE(input.take(TIME_TICK));   // тик истёк, импульсов не было

    input.on_front(LOW_LEVEL);
    input.on_front(HIGH_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));
}

TEST(Electronic, TakeWithoutFrontReturnsFalse)
{
    // Тики времени сами по себе импульсов не создают
    ACTIVE_LOW(input);

    EXPECT_FALSE(input.take(TIME_TICK));
    EXPECT_FALSE(input.take(TIME_TICK));
    EXPECT_FALSE(input.take(NO_TICK));
}

TEST(Electronic, PolarityChangeClearsLatch)
{
    // Смена типа входа не должна отдавать импульс, защёлкнутый прошлым
    ACTIVE_LOW(input);

    input.on_front(LOW_LEVEL);
    input.enable(true);

    EXPECT_FALSE(input.take(NO_TICK));
}

TEST(Electronic, ReturnToSameTypeClearsLatch)
{
    // Электронный -> механический -> электронный: защёлка с прошлого раза чужая
    ACTIVE_LOW(input);

    input.on_front(LOW_LEVEL);
    input.disable();
    input.enable(false);

    EXPECT_FALSE(input.take(NO_TICK));
}

TEST(Electronic, SameTypeKeepsLatch)
{
    /*
    loop() включает вход заново в начале каждого сна. Фронт, пришедший между
    последним опросом сеанса и этим включением, обязан дожить до take().
    */
    ACTIVE_LOW(input);

    input.on_front(LOW_LEVEL);
    input.enable(false);

    EXPECT_TRUE(input.take(NO_TICK));
}

TEST(Electronic, SameTypeKeepsDeadTime)
{
    // Повторное включение не обнуляет и мёртвое время: пачка наводок у
    // ELECTRONIC_HIGH не становится вторым импульсом на границе сеанса
    ACTIVE_HIGH(input);

    input.on_front(HIGH_LEVEL);
    EXPECT_TRUE(input.take(NO_TICK));

    input.enable(true);
    input.on_front(LOW_LEVEL);
    input.on_front(HIGH_LEVEL);
    EXPECT_FALSE(input.take(NO_TICK));
}

TEST(Electronic, IdleLevelAtStartDoesNotCount)
{
    /*
    Первый фронт после включения входа приходит с уровнем покоя — например
    линия отпущена. Импульсом это не считается.
    */
    ACTIVE_LOW(low);
    ACTIVE_HIGH(high);

    low.on_front(HIGH_LEVEL);
    high.on_front(LOW_LEVEL);

    EXPECT_FALSE(low.take(NO_TICK));
    EXPECT_FALSE(high.take(NO_TICK));
}
