#include <gtest/gtest.h>
#include "electronic.h"

/*
Подсчёт импульсов электронного выхода (issue #379).

Импульс газового счётчика Гранд длится 0,7–1,5 мс. Главный цикл attiny за это
время не успевает: он просыпается из сна, стоит в очереди за записью EEPROM и
обменом с ЕСП. Поэтому решение «был импульс» принимается по фронту в
прерывании, а не по уровню, прочитанному позже.

Ниже FRONT — это вызов из ISR с уровнем пина, TAKE — забор результата главным
циклом. Выход с открытым стоком: покой высокий, импульс замыкает на минус.
*/

static const bool HIGH_LEVEL = true;
static const bool LOW_LEVEL = false;

/*
Копировать ElectronicInput нельзя: volatile-поле удаляет конструктор копии.
Поэтому вход настраивается на месте, а не возвращается фабрикой.
*/
#define ENABLED(name) ElectronicInput name; name.enable()

TEST(Electronic, ShortPulseIsNotLost)
{
    /*
    Суть #379: импульс кончился раньше, чем главный цикл проснулся. Оба
    фронта пришли до take(), и уровень к этому моменту снова в покое —
    импульс всё равно обязан быть засчитан.
    */
    ENABLED(input);

    input.on_front(LOW_LEVEL);   // начало импульса
    input.on_front(HIGH_LEVEL);  // конец импульса, 0,7 мс спустя

    EXPECT_TRUE(input.take());
}

TEST(Electronic, PulseCountedOnceUntilIdle)
{
    // Дребезг внутри импульса не должен превращаться в несколько импульсов
    ENABLED(input);

    input.on_front(LOW_LEVEL);
    input.on_front(LOW_LEVEL);
    input.on_front(LOW_LEVEL);

    EXPECT_TRUE(input.take());
    EXPECT_FALSE(input.take());
}

TEST(Electronic, CountsFallingEdge)
{
    ENABLED(input);

    input.on_front(HIGH_LEVEL);  // покой, импульса нет
    EXPECT_FALSE(input.take());

    input.on_front(LOW_LEVEL);
    EXPECT_TRUE(input.take());
}

TEST(Electronic, SecondPulseCountedAfterReturnToIdle)
{
    ENABLED(input);

    input.on_front(LOW_LEVEL);
    EXPECT_TRUE(input.take());

    input.on_front(HIGH_LEVEL);
    EXPECT_FALSE(input.take());

    input.on_front(LOW_LEVEL);
    EXPECT_TRUE(input.take());
}

TEST(Electronic, FastPulsesAllCounted)
{
    /*
    Электросчётчик 3200 имп/кВт·ч на 22 кВт - это 20 импульсов в секунду, а тик
    опроса четыре раза в секунду. Каждый импульс считается и без тика между ними.
    */
    ENABLED(input);

    for (int i = 0; i < 20; i++)
    {
        input.on_front(LOW_LEVEL);
        EXPECT_TRUE(input.take()) << "импульс " << i;
        input.on_front(HIGH_LEVEL);
        EXPECT_FALSE(input.take());
    }
}

TEST(Electronic, TakeWithoutFrontReturnsFalse)
{
    ENABLED(input);

    EXPECT_FALSE(input.take());
    EXPECT_FALSE(input.take());
}

TEST(Electronic, ReturnToElectronicClearsLatch)
{
    // Электронный -> механический -> электронный: защёлка с прошлого раза чужая
    ENABLED(input);

    input.on_front(LOW_LEVEL);
    input.disable();
    input.enable();

    EXPECT_FALSE(input.take());
}

TEST(Electronic, SameTypeKeepsLatch)
{
    /*
    loop() включает вход заново в начале каждого сна. Фронт, пришедший между
    последним опросом сеанса и этим включением, обязан дожить до take().
    */
    ENABLED(input);

    input.on_front(LOW_LEVEL);
    input.enable();

    EXPECT_TRUE(input.take());
}

TEST(Electronic, IdleLevelAtStartDoesNotCount)
{
    /*
    Первый фронт после включения входа приходит с уровнем покоя — например
    линия отпущена. Импульсом это не считается.
    */
    ENABLED(input);

    input.on_front(HIGH_LEVEL);

    EXPECT_FALSE(input.take());
}
