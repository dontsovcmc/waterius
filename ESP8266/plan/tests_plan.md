# План: тесты бизнес-логики ESP

Дата: 2026-08-16

## Зачем

Перед правками багов из трекера нужна сеть безопасности: тесты, которые
фиксируют текущее поведение прошивки. Тогда при фиксе видно, что изменилось
ровно то, что чинили, и не поехало соседнее.

## Текущее состояние: тестов нет

В репозитории есть каталог `ESP8266/test/test_ota` и окружение `[env:native]`,
но **тесты не выполняются**:

```
$ pio test -d ESP8266 -e native
Collected 1 tests
================== 0 test cases: 0 succeeded in 00:00:00.000 ==================
```

Два дефекта:

1. `test_filter = test_ota/*` не совпадает с именем сюита `test_ota`.
   PlatformIO помечает сюит SKIPPED и выходит с кодом 0 — снаружи выглядит
   как «тесты прошли».

2. С правильным фильтром (`pio test -e native -f test_ota`) сборка падает:

   ```
   In file included from src/ota_parse.h:5:
   src/setup.h:4:10: fatal error: 'Arduino.h' file not found
   ```

   `ota_parse.h` тянет `setup.h`, а тот — `<Arduino.h>`, которого на хосте нет.

CI тоже нет: `.github/workflows` пустой, `.travis.yml` проверяет только сборку
и только ветку `master`, то есть PR в `dev` не проверяются ничем.

## Границы

**Тестируем:** бизнес-логику ESP8266 — расчёт показаний, период пробуждения,
правила пользовательского ввода, выбор транспортов.

**Не тестируем:**

- прошивку Attiny85 — отдельная задача;
- семантику Arduino (`String::toFloat`, `IPAddress::fromString`) — вместо этого
  выносим бизнес-правило в чистую функцию, чтобы Arduino в тестируемом коде
  не было вовсе;
- работу с железом: EEPROM, Wi-Fi, I2C, MQTT-сокеты, веб-сервер;
- HIL-тесты на реальном устройстве (`Utils/tests/METF`) — существуют отдельно
  и запускаются вручную.

Первая волна — только характеризация. Все тесты зелёные, поведение прошивки
не меняется ни на бит.

## Архитектура

### Чистое ядро в `src/core/`

Бизнес-логика переезжает в `ESP8266/src/core/` — обычный C++ без `Arduino.h`,
без `String`, без глобальных переменных: на входе `const char*` и POD-структуры,
на выходе значения. Остальной `src/` становится адаптером, который эти функции
вызывает.

```
ESP8266/src/
├── main.cpp, config.cpp, json.cpp, utils.cpp,   ← адаптеры: EEPROM, WiFi,
│   portal/, ha/, senders/                          AsyncWebServer, PubSubClient
└── core/                                        ← чистый C++, тестируется на хосте
    ├── types.h        Settings, AttinyData, CalculatedData, enum'ы
    ├── readings.h/.cpp
    ├── wakeup.h/.cpp
    ├── input.h/.cpp
    └── routing.h/.cpp
```

`Settings`, `AttinyData`, `CalculatedData`, `CounterName`, `CounterType`,
`DataType` уже POD — переезжают в `core/types.h`, а `setup.h` и `master_i2c.h`
его инклюдят. Это ключевой шов: после него `readings`, `wakeup`, `routing`
выносятся почти копипастой, из Arduino в них остаются только макросы `LOG_*`
(в ядре не нужны).

Инклюды в адаптерах и тестах: `#include "core/readings.h"`.

### Окружения для хостовых тестов

Прошивки две, и различаются они компайл-таймово (`#if WATERIUS_MODEL`), поэтому
тестовых окружения тоже два — общая часть вынесена в `[native_base]`:

```ini
[native_base]
platform = native
test_framework = googletest
test_build_src = yes
build_src_filter = -<*> +<core/*>
test_filter = test_*
lib_deps = ArduinoJson@7.3.1

[env:native_classic]           ; Waterius Classic (ESP-01)
extends = native_base
build_flags = -std=c++17 -DFIRMWARE_VERSION=${this.firmware_version} -DWATERIUS_MODEL=0

[env:native_2]                 ; Waterius-2 (ESP-12F)
extends = native_base
build_flags = -std=c++17 -DFIRMWARE_VERSION=${this.firmware_version} -DWATERIUS_MODEL=2
```

Прогон: `pio test -d ESP8266 -e native_classic -e native_2`. Окружения
приходится перечислять явно — голый `pio test` берёт `default_envs`
(`esp01_1m`).

По умолчанию PlatformIO **не собирает `src/` при `pio test`**
(`test_build_src = no`), поэтому ядро с `.cpp`-файлами не слинкуется.
`build_src_filter` оставляет от `src/` только `core/` — иначе в тестовый
бинарник поедут `main.cpp`, `wifi_helpers.cpp` и прочее, что на хосте не
соберётся. Прошивочные окружения фильтр не трогают, у них `src_filter` по
умолчанию, ядро собирается вместе с остальным кодом.

`test_filter = test_*` вместо `test_ota/*` — чинит молчаливый скип.

В прошивочных окружениях (`esp01_1m`, `waterius_2`, `nodemcuv2`) стоит
`test_ignore = *`: после починки `test_filter` без него `pio test` полез бы
собирать тесты под ESP и заливать их в железо.

На сегодня в `src/core` нет ни одного `#if WATERIUS_MODEL`, то есть оба
окружения дают одинаковый результат. Второе окружение заведено на вырост:
в payload поле `voltage_cal` есть только у Waterius-2, и это расхождение
приедет в ядро вместе с `test_payload`. Чтобы прогоны не оказались молча
одинаковыми из-за забытого флага, сюит `test_model` не компилируется без
`-DWATERIUS_MODEL` (неопределённый макрос в `#if` считается нулём, то есть
тихо превратился бы в Classic).

### Структура тестов

```
ESP8266/test/
├── test_readings/     main.cpp + test_readings.cpp
├── test_wakeup/
├── test_input/
├── test_routing/
├── test_model/        сторож: тесты обязаны гоняться под обе модели
└── test_ota/          существующий, чинится
```

Каждый сюит — отдельный каталог со своим `main.cpp`
(`InitGoogleTest` + `RUN_ALL_TESTS`), как уже сделано в `test_ota`.

## Сюиты первой волны

Issue в скобках — что этот тест прикроет, когда до бага дойдут руки.

### 1. `test_readings` — расчёт показаний

Источник: `calculate_values()` (`config.cpp:258`) → `core/readings.cpp`.
Плюс `get_auto_factor()` (`portal/active_point_api.cpp:43`) — уже чистая функция.

Две разные формулы, которые легко перепутать:

| Тип | `factor` | Показания | Дельта |
|---|---|---|---|
| Вода/газ/тепло | литров на импульс | `start + (imp − imp_start)/1000 · factor` (м³) | `(imp − imp_prev) · factor` (литры) |
| Электричество | импульсов на кВт·ч | `start + (imp − imp_start)/factor` (кВт·ч) | `(imp − imp_prev)/factor` (кВт·ч) |

Тесты:

- `WaterFactorConvertsLitresToCubicMeters` — 10 л/имп, 300 импульсов → +3.0 м³
- `WaterDeltaStaysInLitres` — дельта воды в литрах, не в м³ (асимметрия с электро)
- `ElectroFactorIsImpulsesPerKwh` — 1000 имп/кВт·ч, 500 импульсов → 0.5 кВт·ч
- `ElectroDeltaInKwh`
- `ZeroFactorLeavesChannelUntouched` — `factor == 0` → канал не пересчитывается
- `CounterRollbackResetsStart` — `impulses < impulses_start` → `impulses_start`
  сбрасывается, показания не улетают в миллионы
- `FirstCycleDeltaIsNotWholeVolume` — `impulses_previous == 0` на первом цикле
- `NearUint32MaxImpulses` — импульсы близко к `UINT32_MAX`
- `FractionalStartKeepsPrecision` — `channel_start = 123.45`
- `ChannelsAreIndependent` — пересчёт канала 0 не трогает канал 1 (#319)
- `AutoFactorSmallConsumptionGives10` / `AutoFactorLargeConsumptionGives1` —
  `get_auto_factor`, порог `IMPULS_LIMIT_1` (#327, #339)
- `AutoFactorAsColdChannelCopiesCold` — режим `AS_COLD_CHANNEL`

### 2. `test_wakeup` — период пробуждения

Источник: `tune_wakeup()` (`config.cpp:325`) и `reset_period_min_tuned()`
→ `core/wakeup.cpp`. Функция уже чистая, из Arduino только `LOG_INFO`.

Сигнатура (после #357 добавилось число проспанных периодов):
`WakeupTune tune_wakeup(time_t now, time_t base_time, time_t measured_from, uint16_t wakeup_per_min, uint16_t period_min_tuned, uint16_t periods_slept)`

- `PerfectSleepKeepsPeriod` — Attiny отработал ровно → период не дрейфует
- `AttinyRunsFastCorrectsUp` / `AttinyRunsSlowCorrectsDown` — расхождение ±10%
- `MissedSeveralWakeupsDoesNotBreakK` — проспали 2–3 периода (не было
  интернета), `k_estimated` не уезжает (#347)
- `TargetTrapSkipsToNextSlot` — до целевой точки < 1 мин → целимся в следующую
- `TargetCloserThan30PercentSkips` — вторая ветка защиты от «ловушки»
- `Period15Min` / `Period60Min` / `Period720Min` / `Period1440Min`
- `ResultFitsUint16AndIsPositive` — результат всегда > 0 и влезает в `uint16_t`
- `CorrectionNeverExceeds30Percent` — инвариант из комментария в коде
- `ResetTunedPeriodIs90Percent` — смена периода пользователем → `0.9 ×`

Невалидное время (NTP не пришёл) проверяется на уровне адаптера
`update_config()` — в ядро не попадает, `tune_wakeup` вызывается только с
валидным `now`. Тестом фиксируем сам контракт: при `now == last_send`
результат остаётся вменяемым (#357).

### 3. `test_input` — правила пользовательского ввода

Источник: перегрузки `save_param()`, `strncpy_trimmed()`, `is_all_asterisks()`
(`portal/active_point_api.cpp:303-470`) → `core/input.cpp`.

Из `save_param` выносится решение «что считать корректным значением», в
адаптере остаётся только распаковка `AsyncWebParameter` и запись ошибки в
JSON. Коды ошибок сохраняются как есть — их разбирает веб-интерфейс:
`14` — превышена длина, `15` — неверное значение, `17` — пустое значение.

```cpp
// core/input.h
enum class ParseError : uint8_t { NONE = 0, LENGTH = 14, VALUE = 15, EMPTY = 17 };

ParseError parse_decimal(const char *s, float &out);
ParseError parse_uint16(const char *s, uint16_t &out);
ParseError parse_uint8(const char *s, uint8_t &out, bool zero_ok);
ParseError parse_bool(const char *s, uint8_t &out);
ParseError parse_ipv4(const char *s, uint32_t &out);
bool       is_masked(const char *s);                        // "****" — поле не редактировали
ParseError copy_trimmed(char *dst, const char *src, size_t size, bool required);
```

- `CommaAndDotAreSameNumber` — `"12,345"` и `"12.345"` дают одно значение (#332)
- `NoSeparatorIsCharacterized` — `"12345"` фиксируем текущий результат;
  при фиксе #353 будет видно, что именно поменялось
- `GarbageRejected` — `""`, `"abc"`, `"12 345"`, `"12.3м3"`, `"--1"`
- `NegativeReadingRejected` — `"-5"`
- `MaskedValueDetected` — `"****"`, `"* *"`, `"\t*"` → `is_masked() == true`
- `EmptyIsNotMasked` — `""` → `false` (иначе пустое поле никогда не очистить)
- `TrimsLeadingAndTrailingWhitespace` — `"  abc  "` → `"abc"`
- `TooLongValueGivesLengthError` — длина ≥ размера буфера → `14` (#329)
- `EmptyRequiredGivesEmptyError` — `17`, необязательное поле — `NONE`
- `BoolAcceptsZeroAndOne` / `BoolRejectsTwo` → `15`
- `Uint16RejectsZero` — текущее поведение: `0` считается ошибкой
- `Uint8ZeroOkFlag` — обе ветки `zero_ok`
- `ValidIpParsed` / `IpOutOfRangeRejected` (`192.168.0.256`) / `EmptyIpRejected`
- `RejectedValueLeavesTargetUnchanged` — при ошибке поле не перезаписывается

### 4. `test_routing` — какие транспорты отправляют данные

Источник: `is_waterius_site()`, `is_http()`, `is_mqtt()`, `is_ha()`
(`utils.cpp:136-186`) и структура `send_data()` (`senders/send_data.cpp`)
→ `core/routing.cpp`.

```cpp
// core/routing.h
struct Transports { bool waterius; bool http; bool mqtt; bool ha; };
Transports enabled_transports(const Settings &sett);
```

- `AllOffSendsNothing`
- `WateriusOffMqttStillSends` — главный кейс (#320): выключенная передача на
  waterius.ru не должна выключать MQTT
- `WateriusNeedsHostAndKey` — `waterius_on` при пустом `waterius_host`
  или пустом `waterius_key` → выключен
- `HttpNeedsUrl` — `http_on` при пустом `http_url` → выключен
- `MqttNeedsHost` — `mqtt_on` при пустом `mqtt_host` → выключен
- `HaRequiresMqtt` — `mqtt_auto_discovery` при выключенном MQTT → HA выключен
- `HaRequiresAutoDiscovery` — MQTT включён, `mqtt_auto_discovery == 0` → HA выключен
- `AllOnSendsEverywhere`

## CI

`.github/workflows/ci.yml`, триггеры: PR в `dev` и `master`, push в `master`.

```yaml
jobs:
  build:      # matrix: esp01_1m, waterius_2 — pio run -d ESP8266 -e <env>
  build-attiny:  # pio run -d Attiny85
  test:       # pio run: pio test -d ESP8266 -e native
```

Кэшируется `~/.platformio`. `.travis.yml` удаляется — он смотрит только в
`master` и проверяет лишь сборку.

Отдельно проверяем, что харнесс не «зеленеет» вхолостую: шаг `test` падает,
если в выводе `0 test cases`.

## Порядок работ

| PR | Содержание | Меняет поведение | Тестов |
|----|------------|------------------|--------|
| 1 | `core/types.h`, правка `[env:native]`, починенный `test_ota`, CI, удаление `.travis.yml` | нет | 11 |
| 2 | `core/readings.*` + `test_readings` | нет | 18 |
| 3 | `core/wakeup.*` + `test_wakeup` | нет | 16 |
| 4 | `core/input.*` + `test_input` | нет | 32 |
| 5 | `core/routing.*` + `test_routing` | нет | 13 |
| 6 | два окружения `native_classic` / `native_2`, `test_ignore` в прошивочных, `test_model` | нет | 2 |

Каждый PR идёт в `dev` и проходит CI. Только после PR 5 начинаются правки
багов, и каждый фикс сопровождается тестом, который падает до фикса.

Итого 93 теста × 2 модели = 186 прогонов, около 17 секунд.

### Что сделано иначе, чем планировалось

1. **`enabled_transports()` не появилась.** Планировалась структура
   `Transports`, но в прошивке её никто бы не вызвал: решение о транспорте
   каждый sender принимает сам через `is_*()`. Вместо структуры тестируется
   матрица самих предикатов — покрытие то же, дохлого кода нет.

2. **Разбор IP остался в адаптере.** Его делает `IPAddress::fromString()` из
   Arduino. Повторять его в ядре — значит проверять свою копию чужой
   реализации; тест бы ничего не доказывал про устройство.

3. **Тесты гоняются под обе модели.** Изначально планировалось одно
   окружение `native`. Прошивки различаются компайл-таймово, поэтому
   окружений два, а `test_model` не даёт им молча слиться в одно.

4. **Нейтральность PR 1 проверена бинарно.** Секции `text/data/bss` совпали
   до байта, все 5854 символа ELF идентичны по имени, размеру и типу. Дальше
   так уже не выйдет — вынос функций меняет генерацию кода: PR 2 стоил
   +96 байт флеша, PR 4 вернул 16.

### Найденное по дороге

Тесты фиксируют, но не чинят:

- показания: дельта электричества теряет дробную часть кВт·ч (`uint32_t`);
  спец. значения веса импульса 3 и 7 перекрывают настоящие 3 и 7 л/имп;
- ввод: `"abc"` в поле показаний молча становится `0.0` и затирает
  введённое; `"12 345"` читается как `12`; `70000` в `uint16_t` молча
  превращается в `4464` (например, порт MQTT); `"-1"` в флажке проходит
  проверку и даёт `255`, то есть «включено»; длина поля проверяется до
  обрезки пробелов;
- `config.h` объявлял `tune_wakeup` с тремя параметрами вместо пяти —
  объявление разошлось с реализацией и никем не использовалось (исправлено);
- поправка частоты attiny считалась по одному периоду, даже когда устройство
  проспало несколько: дробный остаток принимался за уход частоты, и после
  двух суток без связи расписание уезжало на 4 часа (#345, #347). Починено
  вместе с #357: теперь число проспанных периодов известно точно.

### Сюиты, добавленные после первой волны

- `test_time` — валидность времени, `clock_before_sync`, разбор ответа NTP,
  обход пула серверов; после #357 туда же приехало расписание синхронизаций:
  срок, отступ после неудач, прогрев, оценка времени между синхронизациями
- `test_settings` — раскладка `Settings` в EEPROM: размер 960, смещения полей,
  новые поля только из хвостового резерва. Конфигурация хранится сырыми
  байтами, поэтому сдвиг поля означает, что каждое прошитое устройство
  прочитает чужие данные
- `test_url` — разбор адреса брокера MQTT (#330)

## Вторая волна (за границами этой задачи)

Список зафиксирован, чтобы не потерять:

- `test_payload` — `get_json_data()`: набор ключей, `data_type` по типу
  счётчика, отключённый канал, каналы не перепутаны (#360, #319)
- `test_ha` — discovery: сущности по типам счётчиков, тепло (#356),
  `default_entity_id` (#351), публикация `0.0` (#325), стабильность `unique_id`
- `test_config` — `get_checksum`, версия конфигурации, набор полей,
  переживающих factory reset (#326, #344). Раскладка и размер уже покрыты
  сюитом `test_settings`

## Правила для новых тестов

- Тест живёт в `test/test_<модуль>/`, имя — `<Модуль><ЧтоПроверяем>`.
- Тестируется только `src/core/` — если для теста понадобился `Arduino.h`,
  значит логику надо сначала вынести в ядро.
- Один тест — одно правило. Матрицы (транспорты, типы счётчиков) пишутся
  параметризованными тестами, а не циклом внутри одного `TEST`.
- Характеризационный тест, который фиксирует сомнительное поведение,
  помечается комментарием со ссылкой на issue.
