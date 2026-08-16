#include <gtest/gtest.h>
#include "core/types.h"

/*
Сторож для двух тестовых окружений.

Прошивки две, различия между ними компайл-таймовые. Тесты гоняются под
каждую модель: pio test -d ESP8266 запускает native_classic и native_2.
Без этого сюита забытый флаг -DWATERIUS_MODEL остался бы незамеченным:
оба прогона молча собрали бы один и тот же код, а зелёный CI создавал бы
впечатление, что обе прошивки проверены.

Неопределённый макрос в #if считается нулём, то есть тихо превращается в
WATERIUS_MODEL_1. Поэтому проверка именно на "определён", а не на значение.
*/

#ifndef WATERIUS_MODEL
#error "Не задан -DWATERIUS_MODEL. Тесты обязаны гоняться под каждую модель: native_classic и native_2."
#endif

TEST(Model, FlagIsOneOfKnownModels)
{
    EXPECT_TRUE(WATERIUS_MODEL == WATERIUS_MODEL_1 || WATERIUS_MODEL == WATERIUS_MODEL_2)
        << "WATERIUS_MODEL=" << WATERIUS_MODEL;
}

TEST(Model, ModelsAreDistinct)
{
    EXPECT_NE(WATERIUS_MODEL_1, WATERIUS_MODEL_2);
}
