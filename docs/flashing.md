# Прошивка платы по проводам

Готовый порядок действий: определить, собрать, записать, убедиться. Всё, что
ниже, проверено на живой плате — команды можно копировать как есть.

Про обновление по воздуху здесь ничего нет: это `ESP8266/scripts/build_and_deploy.sh`
и [ota-and-reset.md](ota-and-reset.md).

## Что нужно

| Чем | Что прошивает | Как выглядит в системе |
|---|---|---|
| USBasp | attiny85 | `system_profiler SPUSBDataType \| grep -i usbasp` |
| UART-адаптер (FT232R, CP2102) | ЕСП | `/dev/cu.usbserial-*` |

`avrdude` и `esptool` ставить не надо, они уже есть внутри PlatformIO:

```shell
~/.platformio/packages/tool-avrdude/bin/avrdude
~/.platformio/penv/bin/python -m esptool
```

**Конфиг avrdude лежит не в `etc/`,** а рядом с пакетом, и без `-C` avrdude
падает на «can't open config file»:

```shell
~/.platformio/packages/tool-avrdude/avrdude.conf
```

## 1. Порт и модель

Портов в списке много, нужен тот, за которым стоит USB-UART:

```shell
ls /dev/cu.*
system_profiler SPUSBDataType | grep -B2 -A6 "FT232\|CP210"
```

Модель определяется размером флеша, а не на глаз — от неё зависят и окружение
сборки, и адрес файловой системы:

```shell
~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 115200 flash_id
```

| Detected flash size | Модель | Окружение ЕСП | Окружение attiny | ФС |
|---|---|---|---|---|
| 4MB | Ватериус-2 (ESP-12F) | `waterius_2` | `waterius_2` | `0x300000` |
| 1MB | Классик (ESP-01) | `esp01_1m` | `attiny85` | `0xBB000` |

Определив модель, берите **целиком** соответствующий рецепт из раздела 2 и не
смешивайте команды из разных: окружение у attiny и у ЕСП обязано быть от одной
модели.

## 2. Два готовых рецепта

Заполнять здесь нечего, кроме `<PORT>`: окружение стоит в самом пути, поэтому
перепутать модель, копируя команду целиком, нельзя. Порядок внутри рецепта
менять нельзя — сначала attiny, потом ЕСП: ЕСП при старте читает у attiny
версию и данные.

### Ватериус-2 (ESP-12F, `Detected flash size: 4MB`)

```shell
~/.platformio/penv/bin/pio run -d Attiny85 -e waterius_2
~/.platformio/penv/bin/pio run -d ESP8266  -e waterius_2
~/.platformio/penv/bin/pio run -d ESP8266  -e waterius_2 -t buildfs

~/.platformio/packages/tool-avrdude/bin/avrdude \
  -C ~/.platformio/packages/tool-avrdude/avrdude.conf \
  -p t85 -c usbasp -B 4 -P usb \
  -U flash:w:"Attiny85/.pio/build/waterius_2/firmware.hex":i

~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 460800 \
  write_flash --flash_freq 40m --flash_size 4MB --flash_mode qio \
  0x0      ESP8266/.pio/build/waterius_2/firmware.bin \
  0x300000 ESP8266/.pio/build/waterius_2/littlefs.bin
```

### Классик (ESP-01, `Detected flash size: 1MB`)

```shell
~/.platformio/penv/bin/pio run -d Attiny85 -e attiny85
~/.platformio/penv/bin/pio run -d ESP8266  -e esp01_1m
~/.platformio/penv/bin/pio run -d ESP8266  -e esp01_1m -t buildfs

~/.platformio/packages/tool-avrdude/bin/avrdude \
  -C ~/.platformio/packages/tool-avrdude/avrdude.conf \
  -p t85 -c usbasp -B 4 -P usb \
  -U flash:w:"Attiny85/.pio/build/attiny85/firmware.hex":i

~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 460800 \
  write_flash --flash_freq 40m --flash_size 1MB --flash_mode qio \
  0x0     ESP8266/.pio/build/esp01_1m/firmware.bin \
  0xBB000 ESP8266/.pio/build/esp01_1m/littlefs.bin
```

### Почему рецепта два, а не один с подстановкой

Прошивки различаются на этапе компиляции, и **у attiny тоже**. У ЕСП это
`WATERIUS_MODEL`: линия i2c на GPIO0/GPIO2 против GPIO4/GPIO5
(`ESP8266/src/master_i2c.h`), светодиоды и кнопка (`ESP8266/src/setup.h`).
У attiny тот же флаг разводит вещи, от которых зависит, оживёт ли плата вообще:

| | Классик (`attiny85`) | Ватериус-2 (`waterius_2`) |
|---|---|---|
| Включение питания ЕСП | HIGH | **LOW** (`Attiny85/src/Power.cpp`, ESPPowerPin::power) |
| Кнопка | PB2 | **PB1** (`Attiny85/src/main.cpp`) |
| Пороги компаратора входов | под 3к3+300 Ом | под 1 кОм (`Attiny85/src/counter.h`) |

**`default_envs` в `Attiny85/platformio.ini` — это `attiny85`, то есть
классик.** Поэтому `pio run -d Attiny85` без `-e` собирает прошивку классика
молча и успешно, и на Ватериусе-2 она даёт мёртвую плату: attiny не считает PB1
кнопкой, сеанс не заказывает, на i2c не отвечает. Как это выглядит снаружи —
в разборе неисправностей ниже.

## 3. Адреса и имена, если рецепт не подошёл

Адрес ФС берётся из `board_build.ldscript` в `ESP8266/platformio.ini`:
`_FS_start` минус `0x40200000`.

Кроме каталогов сборки, образы раскладываются в корни проектов под именами с
версией (`objdump.py`, `post_compile.py`) — версия у attiny в
`Attiny85/platformio.ini`, у ЕСП в `ESP8266/platformio.ini`:

| Файл | Что это |
|---|---|
| `Attiny85/<окружение>-<версия>.hex` | прошивка attiny; с `LOG_ON` имя получает `-log` |
| `ESP8266/<окружение>-<версия>.bin` | прошивка ЕСП |
| `ESP8266/<окружение>-<версия>-fs.bin` | образ LittleFS |
| `ESP8266/<окружение>-<версия>-full.bin` | оба образа в одном файле с адреса `0x0` — для ESP Web Tools |

Если в локальном `secrets.ini` (шаблон — [ESP8266/secrets.ini.template](../ESP8266/secrets.ini.template)) задан `WIFI_SSID`, к имени добавляется `-test`.

Фьюзы attiny (ставятся один раз, при первой прошивке чипа): `E:FF, H:DF, L:62`.
EESAVE в них не запрограммирован, поэтому **каждая прошивка attiny стирает её
EEPROM**: счётчики импульсов начинаются с нуля. Показания счётчиков воды это не
затрагивает — они в настройках ЕСП.

## 4. Почему ЕСП пишется одной командой

**Двумя отдельными командами не получится.** После записи прошивки ЕСП
перезагружается и уходит в свой цикл i2c и сна, автосброс больше не роняет её в
загрузчик, и вторая запись умирает на `Failed to connect to ESP8266: Timed out
waiting for packet header`. Одна команда держит чип в загрузчике между записями.

По той же причине не годятся `pio run -t upload` и следом `-t uploadfs`.

## 5. Чем доказывается успех

Ровно двумя вещами:

```
avrdude: NNNN bytes of flash verified
...
Wrote  NNNNNN bytes at 0x00000000 …  Hash of data verified.
Wrote  NNNNNN bytes at <адрес ФС> …  Hash of data verified.
```

Всё, прошивка записана. Больше проверять нечего.

**«Verified» доказывает запись, а не выбор файла.** Прошивка не той модели
пишется и сверяется так же успешно, как своя: avrdude сверяет чип с тем файлом,
который ему дали, и о том, что файл чужой, не скажет никогда. Защищает от этого
только одно - окружение, стоящее прямо в пути (`.pio/build/waterius_2/` против
`.pio/build/attiny85/`). Поэтому рецепт и копируется целиком, а не собирается
из кусков.

**Слушать UART после прошивки бессмысленно.** Питанием ЕСП распоряжается attiny
и между сеансами держит его выключенным, а сброс по RTS сеанса не заказывает —
порт будет молчать и на исправной плате. Хотите увидеть лог — это отдельное
действие: нажать кнопку на корпусе и только тогда читать порт на 115200.

## Если не подключается

| Симптом | Причина |
|---|---|
| `Failed to connect to ESP8266` | плата не в загрузчике: у Ватериуса-2 ЕСП запитывает attiny, дайте питание и держите GPIO0 на земле |
| `can't open config file` у avrdude | забыт `-C`, см. путь выше |
| `command not found: avrdude` | берите из пакетов PlatformIO, в PATH его нет |
| `Detected flash size` не совпал с ожидаемой моделью | модель другая: адрес ФС и окружение сборки тоже другие |
| После прошивки: удержание кнопки ничем не подсвечивается, затем 5 вспышек красного | ЕСП не получила ответа attiny по i2c. `main.cpp:loop` пропускает весь блок, `config_loaded` остаётся false, `blynk_error(ERROR_CONFIG=5)`. Первое, что проверить, - не залита ли в attiny прошивка чужой модели |
