#ifndef _ELECTRONIC_h
#define _ELECTRONIC_h

/*
Подсчёт импульсов электронного выхода счётчика.

Импульс здесь короткий: у газового счётчика Гранд это 0,7–1,5 мс (issue #379).
Столько времени главный цикл attiny не имеет: он просыпается из сна, ждёт
своей очереди за записью EEPROM и обменом с ЕСП. Поэтому уровень входа
снимается в самом прерывании по фронту и защёлкивается здесь, а главный цикл
потом просто забирает готовый результат.

Выход с открытым стоком: покой — высокий уровень от подтяжки attiny, импульс —
замыкание на минус.

Файл намеренно без Arduino.h и без обращений к регистрам: правило «что считать
импульсом» проверяется хостовыми тестами (Attiny85/test/test_electronic).
*/

#include <stdint.h>

struct ElectronicInput
{
    /*
    Фронт, защёлкнутый прерыванием. volatile: пишется в ISR, читается в
    главном цикле.
    */
    volatile bool pending;

    /*
    Линия сейчас замкнута. Пока это так, новый импульс не засчитывается:
    один импульс — один переход из покоя.
    */
    bool on_pulse;

    // Включён электронным типом: так enable отличает прежний тип от нового.
    bool enabled;

    ElectronicInput()
        : pending(false), on_pulse(false), enabled(false)
    {
    }

    /*
    Тот же тип - защёлку не трогаем: loop() включает вход в начале каждого сна,
    и фронт с конца сеанса пропал бы. Новый тип - старая защёлка ему чужая.
    */
    void enable()
    {
        if (enabled)
            return;

        enabled = true;
        pending = false;
        on_pulse = false;
    }

    void disable()
    {
        enabled = false;
    }

    /*
    Вызывается из прерывания по фронту. level — уровень пина в этот момент.
    */
    void on_front(const bool level)
    {
        if (level)
        {
            on_pulse = false;
            return;
        }

        if (!on_pulse)
        {
            on_pulse = true;
            pending = true;
        }
    }

    /*
    Вызывается из главного цикла.

    @return true если импульс засчитан
    */
    bool take()
    {
        const bool pulse = pending;
        pending = false;
        return pulse;
    }
};

#endif
