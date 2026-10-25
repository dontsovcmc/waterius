#ifndef _ELECTRONIC_h
#define _ELECTRONIC_h

/*
Подсчёт импульсов электронного выхода счётчика.

Импульс здесь короткий: у газового счётчика Гранд это 0,7–1,5 мс (issue #379).
Столько времени главный цикл attiny не имеет: он просыпается из сна, ждёт
своей очереди за записью EEPROM и обменом с ЕСП. Поэтому уровень входа
снимается в самом прерывании по фронту и защёлкивается здесь, а главный цикл
потом просто забирает готовый результат.

Файл намеренно без Arduino.h и без обращений к регистрам: правило «что считать
импульсом» проверяется хостовыми тестами (Attiny85/test/test_electronic).
*/

#include <stdint.h>

struct ElectronicInput
{
    /*
    Полярность. У выхода с открытым стоком покой — высокий уровень, импульс —
    замыкание на минус. У выхода, который сам выдаёт положительный импульс
    (ELECTRONIC_HIGH), всё наоборот.
    */
    bool active_high;

    /*
    Фронт, защёлкнутый прерыванием. volatile: пишется в ISR, читается в
    главном цикле.
    */
    volatile bool pending;

    /*
    Линия сейчас в активном уровне. Пока это так, новый импульс не
    засчитывается: один импульс — один переход из покоя.
    */
    bool on_pulse;

    /*
    Мёртвое время в тиках опроса (250 мс), только у ELECTRONIC_HIGH: высокоомный
    вход со свободным проводом ловит эфир. Открытому стоку линию держит подтяжка.
    */
    uint8_t dead_time;

    // Включён электронным типом: так enable отличает прежний тип от нового.
    bool enabled;

    ElectronicInput()
        : active_high(false), pending(false), on_pulse(false), dead_time(0), enabled(false)
    {
    }

    /*
    Тот же тип - защёлку не трогаем: loop() включает вход в начале каждого сна,
    и фронт с конца сеанса пропал бы. Новый тип - старая защёлка ему чужая.
    */
    void enable(const bool high)
    {
        if (enabled && active_high == high)
            return;

        enabled = true;
        active_high = high;
        pending = false;
        on_pulse = false;
        dead_time = 0;
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
        const bool active = (level == active_high);

        if (!active)
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
    Вызывается из главного цикла. time_event — сработал тик опроса 250 мс,
    по нему истекает мёртвое время ELECTRONIC_HIGH.

    @return true если импульс засчитан
    */
    bool take(const bool time_event)
    {
        if (time_event && dead_time)
            dead_time--;

        const bool pulse = pending;
        pending = false;

        if (!pulse || dead_time)
            return false;

        if (active_high)
            dead_time = 1;
        return true;
    }
};

#endif
