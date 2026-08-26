#ifndef _ALARM_h
#define _ALARM_h

/*
Детекция аварий по импульсам счётчика (issue #202).

Считать умеет только attiny: ЕСП спит между сеансами и импульсов не видит.
Поэтому правила живут здесь, а ЕСП получает готовое состояние, отдаёт его
наружу и задаёт пороги.

Три независимых состояния, см. docs/alarms.md:

  ALARM_FLOW - большой расход: интервал между импульсами короче порога.
               Прорыв трубы, лопнувший полотенцесушитель.
  ALARM_LEAK - протечка: расход не прекращается дольше заданного времени.
               Капающий кран, текущий бачок.
  ALARM_WET  - датчик протечки на входе замкнут.

Файл намеренно без Arduino.h и без обращений к регистрам: правила
проверяются хостовыми тестами (Attiny85/test/test_alarm).
*/

#include <stdint.h>

#define ALARM_FLOW 0x01
#define ALARM_LEAK 0x02
#define ALARM_WET 0x04

/*
Минут тишины после сеанса по тревоге. Для подтверждённого доклада - защита
от дребезга датчика, для неподтверждённого - пауза до следующей попытки.
*/
#define ALARM_HOLD_MIN 5

/*
Сколько раз будим ЕСП, пока она не подтвердит доставку. Пять попыток по пять
минут - максимум 20 минут на доставку. Быстрее смысла нет: внутри сеанса ЕСП
уже делает три попытки POST и несколько попыток подключения к роутеру.
*/
#define ALARM_MAX_TRIES 5

/*
Тиков сторожевого таймера (250мс) в минуте.
*/
#define ALARM_TICKS_PER_MINUTE 240

struct AlarmDetector
{
    /*
    Настройки от ЕСП, 0 - тревога выключена.

    Пороги считает ЕСП: у неё есть вес импульса и тип счётчика, а деление на
    attiny стоит дорого. Здесь только сравнения.
    */
    uint16_t min_interval; // тиков между импульсами, меньше - большой расход
    uint16_t leak_min;     // минут непрерывного расхода - протечка

    uint16_t ticks;    // тиков с прошлого импульса, с насыщением
    uint16_t prev_gap; // прошлый интервал между импульсами, тиков
    uint16_t run_min;  // минут непрерывного расхода

    uint8_t state : 3;   // маска ALARM_*
    uint8_t changed : 1; // состояние сменилось - есть о чём доложить
    uint8_t held : 1;    // доклад не подтверждён, состояния не снимаем
    uint8_t reserved : 3;

    AlarmDetector()
        : min_interval(0), leak_min(0), ticks(0), prev_gap(0), run_min(0),
          state(0), changed(0), held(0), reserved(0)
    {
    }

    void configure(const uint16_t interval, const uint16_t minutes)
    {
        min_interval = interval;
        leak_min = minutes;
    }

    /*
    Вода идёт: ритм импульсов не сбился.

    Ритм, а не объём - в этом всё отличие от ветки alarm. Окно фиксированной
    длины ловит только расход чаще одного импульса за окно: капающий кран на
    60 л/ч при весе 10 л/имп даёт импульс раз в 10 минут, и при окне в 5 минут
    такая протечка не детектируется никогда. Ритм от скорости не зависит.

    Допуск - вдвое: сравниваем сдвигом, а не умножением, иначе prev_gap * 2
    переполнил бы uint16_t на длинных паузах.
    */
    bool flowing() const
    {
        return prev_gap != 0 && (ticks >> 1) <= prev_gap;
    }

    /*
    Импульс засчитан.
    */
    void on_pulse()
    {
        if (min_interval && ticks < min_interval)
        {
            raise(ALARM_FLOW);
        }

        if (!flowing())
        {
            // Ритм сбился: копим время непрерывного расхода заново
            run_min = 0;
            clear(ALARM_LEAK);
        }

        prev_gap = ticks;
        ticks = 0;
    }

    /*
    Прошло 250мс.
    */
    void on_tick()
    {
        if (ticks < UINT16_MAX)
        {
            ticks++;
        }

        // Расход упал ниже половины порога. Гистерезис вдвое, чтобы тревога
        // не мигала на границе
        if ((state & ALARM_FLOW) && min_interval && (ticks >> 1) > min_interval)
        {
            clear(ALARM_FLOW);
        }
    }

    /*
    Прошла минута. Считается снаружи одним счётчиком на оба канала.
    */
    void on_minute()
    {
        if (!flowing())
        {
            run_min = 0;
            prev_gap = 0;
            clear(ALARM_LEAK);
            return;
        }

        if (run_min < UINT16_MAX)
        {
            run_min++;
        }

        if (leak_min && run_min >= leak_min)
        {
            raise(ALARM_LEAK);
        }
    }

    /*
    Состояние входа с датчиком протечки.
    */
    void set_wet(const bool closed)
    {
        if (closed)
            raise(ALARM_WET);
        else
            clear(ALARM_WET);
    }

    /*
    Пока доклад не подтверждён, состояния не снимаются: иначе тревога успевала
    бы погаснуть до того, как ЕСП прочитает байт флагов, и в отчёт ушло бы
    "всё хорошо".
    */
    void hold(const bool on)
    {
        held = on ? 1 : 0;
    }

private:
    void raise(const uint8_t bit)
    {
        if (state & bit)
            return;
        state |= bit;
        changed = 1;
    }

    void clear(const uint8_t bit)
    {
        if (held || !(state & bit))
            return;
        state &= ~bit;
        changed = 1;
    }
};

/*
Доклад о тревоге: сколько раз будили ЕСП и ждём ли подтверждения.
Один на оба канала - тревоги уезжают одним сеансом.
*/
struct AlarmReport
{
    uint8_t tries : 3;    // 0..ALARM_MAX_TRIES
    uint8_t pending : 1;  // доклад не подтверждён
    uint8_t reserved : 4;

    AlarmReport() : tries(0), pending(0), reserved(0) {}

    /*
    Начинаем новый доклад: счётчик попыток с нуля.
    */
    void start()
    {
        pending = 1;
        tries = 0;
    }

    /*
    ЕСП подтвердила доставку.
    */
    void confirm()
    {
        pending = 0;
        tries = 0;
    }

    /*
    Сеанс закончился без подтверждения.

    @return true если стоит попробовать ещё раз
    */
    bool failed()
    {
        if (!pending)
            return false;

        tries++;
        if (tries >= ALARM_MAX_TRIES)
        {
            // Сдаёмся: состояние уедет со следующим плановым сеансом
            pending = 0;
            return false;
        }
        return true;
    }
};

#endif
