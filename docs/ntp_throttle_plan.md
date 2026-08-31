# NTP раз в сутки + подстройка периода только на синхронизациях (Вариант Б)

## Контекст и решение

Исходная задача: при коротком периоде пробуждения (например, 15 мин) ESP дёргает NTP-пул
~96 раз в сутки ([main.cpp:127-132](../ESP8266/src/main.cpp#L127-L132)). Нужно — не чаще
раза в `NTP_UPDATE_DELAY_MIN` (= 1440) минут.

При разборе выяснилось (см. моделирование в обсуждении), что задача не сводится к «запрашивать
NTP реже»: текущая `tune_wakeup()` — это **замкнутый контур**, который каждый цикл по свежему
времени NTP подстраивает фазу под сетку `base_time + N×период`. Именно он даёт качание
13–14 и **требует реального времени на каждом цикле**. Троттлинг NTP и ежецикловая подстройка
несовместимы.

Назначение этой подстройки — **стабильный каденс ≈ `wakeup_per_min` и защита батареи** (WDT
attiny врёт на ±10–30% от температуры; без коррекции «15 мин» уплывает в 11–20), а **не**
привязка к времени суток (`base_time` — это момент настройки, не полночь).

**Решение (Вариант Б):** `period_min_tuned` **фиксирован между синхронизациями**. Коррекцию
считаем **только на NTP-синхронизации** — по среднему реальному интервалу за всё прошедшее окно.
Это закрывает задачу с нагрузкой, убирает качание 13–14 и упрощает логику. Цена — фаза за сутки
может уехать на десятки минут, но раз в сутки переякоривается (пользователь это принял).

Подтверждённые решения: оценка времени для timestamp приемлема; ручные/первый запуск/невалидное
время — всегда NTP; `NTP_UPDATE_DELAY_MIN` — константа `#define` = 1440.

## Принцип работы

На каждом пробуждении растёт счётчик `wakes_since_ntp`. Пока накопленный номинал
(`wakes_since_ntp × wakeup_per_min`) меньше порога — NTP **не** запрашиваем, только ставим
оценочные часы для timestamp. Когда порог достигнут (или первый запуск / ручной режим) —
делаем реальную синхронизацию и **пересчитываем период по среднему за окно**:

```
real_window_min = (now − last_ntp_sync) / 60     // реальное время, прошедшее за окно
n               = wakes_since_ntp                 // сколько раз проснулись за окно
// реальный интервал за цикл = real_window_min / n; хотим, чтобы он стал wakeup_per_min:
P_new = period_min_tuned × wakeup_per_min × n / real_window_min
```
с ограничением ±30% за раз (защита от выбросов, как нынешний guard `k < 0.7`). После этого
`last_ntp_sync = now`, `wakes_since_ntp = 0`.

**Пример.** `wakeup_per_min`=15, период был P=15, за окно n=96 пробуждений, реально прошло
1296 мин (устройство шло быстро, ~13.5/цикл). `P_new = 15 × 15 × 96 / 1296 = 16.67 → 17`.
В следующем окне attiny спит 17 «кривых» минут ≈ 17×0.9 = 15.3 реальных → каденс вернулся к 15.

## Изменения

### 1. Новая константа — [setup.h](../ESP8266/src/setup.h) рядом с `DEFAULT_WAKEUP_PERIOD_MIN` (~стр. 350)
```c
#ifndef NTP_UPDATE_DELAY_MIN
#define NTP_UPDATE_DELAY_MIN 1440   // не чаще раза в сутки реально запрашиваем NTP
#endif
```

### 2. Новые поля EEPROM — структура Settings в [setup.h](../ESP8266/src/setup.h) (в резервном хвосте)
Добавить после `reserved8`, укоротив `reserved9`, чтобы смещения существующих полей не сдвинулись
и `sizeof(Settings)` остался 960 (страховка — `static_assert` в [main.cpp:41](../ESP8266/src/main.cpp#L41)):
```c
uint8_t  reserved8 = 0;
time_t   last_ntp_sync = 0;        // время последней УСПЕШНОЙ синхронизации NTP (якорь окна)
uint16_t wakes_since_ntp = 0;      // число пробуждений с последней синхронизации
uint8_t  reserved9[64] = {0};      // было [74]
```
`base_time` больше не используется для подстройки — поле оставляем как есть (для совместимости
EEPROM), но в новой логике не читаем/не пишем. `last_send` остаётся «временем последнего
выхода на связь» (пишется каждый цикл), но из подстройки исключается.

### 3. Подстройка периода по окну — [config.cpp](../ESP8266/src/config.cpp) / [config.h](../ESP8266/src/config.h)
Заменить `tune_wakeup(...)` на функцию, считающую среднее за окно:
```c
// Возвращает новый period_min_tuned («кривые минуты attiny»). Вызывается только на NTP-синхронизации.
uint16_t tune_period_window(uint16_t period_min_tuned, uint16_t wakeup_per_min,
                            uint16_t wakes_since_ntp, double real_window_min)
{
    if (wakes_since_ntp == 0 || real_window_min < 1.0 || period_min_tuned == 0)
        return period_min_tuned;                      // нет валидного окна — не трогаем

    double p = (double)period_min_tuned * wakeup_per_min * wakes_since_ntp / real_window_min;

    // Ограничиваем коррекцию ±30% за раз
    double lo = period_min_tuned * 0.7, hi = period_min_tuned * 1.3;
    if (p < lo) p = lo;
    if (p > hi) p = hi;

    uint16_t res = (uint16_t)lround(p);
    LOG_INFO(F("Tune window: n=") << wakes_since_ntp << F(", real_min=") << real_window_min
             << F(", period_min_tuned=") << res);
    return res;
}
```

### 4. Обёртка-троттлинг — [sync_time.cpp](../ESP8266/src/sync_time.cpp) / [sync_time.h](../ESP8266/src/sync_time.h)
```c
static void set_estimated_time(time_t t) { timeval tv = { t, 0 }; settimeofday(&tv, NULL); }

void update_system_time(Settings &sett)
{
    sett.wakes_since_ntp++;                                   // ещё одно пробуждение
    uint32_t nominal_min = (uint32_t)sett.wakes_since_ntp * sett.wakeup_per_min;

    // Оценка времени для timestamp: якорь последней синхронизации + номинал.
    time_t estimated = sett.last_ntp_sync + (time_t)nominal_min * 60;

    bool need_sync = !is_valid_time(sett.last_ntp_sync)        // ещё не синхронизировались
                  || sett.mode != TRANSMIT_MODE                // ручное/настройка — всегда NTP
                  || nominal_min >= NTP_UPDATE_DELAY_MIN;

    if (need_sync)
    {
        if (sync_ntp_time(sett))                               // сам ставит часы по NTP
        {
            time_t now = time(nullptr);
            // Подстройка периода по среднему за окно — только в авторежиме и при валидном якоре.
            if (sett.mode == TRANSMIT_MODE && is_valid_time(sett.last_ntp_sync))
            {
                double real_window_min = difftime(now, sett.last_ntp_sync) / 60.0;
                sett.period_min_tuned = tune_period_window(sett.period_min_tuned,
                        sett.wakeup_per_min, sett.wakes_since_ntp, real_window_min);
            }
            sett.last_ntp_sync   = now;                         // новый якорь окна
            sett.wakes_since_ntp = 0;
            return;
        }
        sett.ntp_error_counter++;
        if (is_valid_time(estimated)) set_estimated_time(estimated);  // откат на оценку, повтор позже
        return;
    }

    set_estimated_time(estimated);                             // NTP пропущен — оценка для timestamp
    LOG_INFO(F("NTP skipped ") << nominal_min << F("/") << NTP_UPDATE_DELAY_MIN << F(" min"));
}
```

### 5. Упростить `update_config()` — [config.cpp:381-422](../ESP8266/src/config.cpp#L381-L422)
Убрать из неё подстройку периода и логику `base_time` (теперь это в обёртке). Оставить обновление
impulses/`wake_time`/`last_send`:
```c
void update_config(Settings &sett, const AttinyData &data, const CalculatedData &cdata)
{
    sett.impulses0_previous = data.impulses0;
    sett.impulses1_previous = data.impulses1;
    if (!sett.period_min_tuned) reset_period_min_tuned(sett);
    sett.wake_time = millis();

    time_t now = time(nullptr);
    if (is_valid_time(now))
        sett.last_send = now;          // «последний выход на связь» (реальное или оценочное)
}
```

### 6. Точка вызова — [main.cpp:126-132](../ESP8266/src/main.cpp#L126-L132)
```c
// устанавливать время только при использовании https или mqtt
if (is_mqtt(sett) || is_https(sett.waterius_host) || is_https(sett.http_url))
{
    update_system_time(sett);
}
```
`update_config(sett, data, cdata)` на [строке 160](../ESP8266/src/main.cpp#L160) и
`setWakeUpPeriod(sett.period_min_tuned)` на [162](../ESP8266/src/main.cpp#L162) — без изменений
по сигнатуре; на пропущенных циклах `period_min_tuned` не меняется → attiny держит прежний период.

## Крайние случаи
- **Первый запуск:** `last_ntp_sync` невалиден → синхронизация без коррекции (нет окна), ставим
  якорь; период остаётся стартовым (`wakeup_per_min × 0.9`), корректируется со следующего окна.
- **Сбой NTP в нужном цикле:** `wakes_since_ntp` не сбрасывается → повтор на следующем пробуждении;
  пока что оценка держит часы.
- **Ручное пробуждение/кнопка:** всегда NTP; окно неполное → коррекцию пропускаем (guard по
  `TRANSMIT_MODE`), просто переякориваемся.
- **Пользователи по умолчанию (`wakeup_per_min` = 1440):** порог достигается на первом
  пробуждении → синхронизация каждый раз, коррекция по окну в один цикл — эквивалент текущего
  поведения по частоте NTP.
- **Защита от переполнения:** `nominal_min` считается в `uint32_t`; `wakes_since_ntp` (uint16)
  при разумных периодах не переполняется (порог достигается задолго до этого).

## Что удаляется/перестаёт работать
- Ежецикловая привязка к `base_time` (сетка времени суток) — больше нет. Каденс держится
  стабильным по среднему за окно, но фаза не привязана к абсолютному расписанию.
- Качание `period_min_tuned` 13–14 исчезает: внутри окна период постоянный.

## Проверка
1. `~/.platformio/penv/bin/pio run -d ESP8266` — компиляция; `static_assert(sizeof(Settings)==960)` проходит.
2. `~/.platformio/penv/bin/pio test -d ESP8266 -e native` — тесты OTA не затронуты (sanity).
   Если для `tune_period_window` добавить host-тест — проверить пример (n=96, real=1296, P:15→17).
3. Стенд, `LOG_LEVEL_INFO`, короткий период (например, 1 мин):
   - Первое пробуждение — реальная NTP; далее `NTP skipped N/1440 min`, пока не наберётся ~1440 мин
     номинала, затем синхронизация с логом `Tune window: ...` и сбросом счётчика.
   - `period_min_tuned` меняется ТОЛЬКО на циклах синхронизации; внутри окна постоянен.
   - Реальный интервал появления в сети стабилен (без 13–14); за сутки фаза слегка уезжает и
     корректируется на следующей синхронизации.
   - `ntp_errors` не растёт при простом пропуске; кнопка форсирует NTP.
