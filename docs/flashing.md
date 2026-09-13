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

| Detected flash size | Модель | Окружение | ФС |
|---|---|---|---|
| 4MB | Ватериус-2 (ESP-12F) | `waterius_2` | `0x300000` |
| 1MB | Классик (ESP-01) | `esp01_1m` | `0xBB000` |

Адрес ФС берётся из `board_build.ldscript` в `ESP8266/platformio.ini`:
`_FS_start` минус `0x40200000`.

## 2. Сборка

```shell
~/.platformio/penv/bin/pio run -d Attiny85
~/.platformio/penv/bin/pio run -d ESP8266 -e <окружение>
~/.platformio/penv/bin/pio run -d ESP8266 -e <окружение> -t buildfs
```

Образы раскладываются в корни проектов под понятными именами (`objdump.py`,
`post_compile.py`):

| Файл | Что это |
|---|---|
| `Attiny85/attiny85-<версия>.hex` | прошивка attiny; с `LOG_ON` имя получает `-log` |
| `ESP8266/<окружение>-<версия>.bin` | прошивка ЕСП |
| `ESP8266/<окружение>-<версия>-fs.bin` | образ LittleFS |
| `ESP8266/<окружение>-<версия>-full.bin` | оба образа в одном файле с адреса `0x0` — для ESP Web Tools |

Если в локальном `secrets.ini` (шаблон — [ESP8266/secrets.ini.template](../ESP8266/secrets.ini.template)) задан `WIFI_SSID`, к имени добавляется `-test`.

## 3. attiny — первой

Сначала attiny, потом ЕСП: ЕСП при старте читает у attiny версию и данные.

```shell
~/.platformio/packages/tool-avrdude/bin/avrdude \
  -C ~/.platformio/packages/tool-avrdude/avrdude.conf \
  -p t85 -c usbasp -B 4 -P usb \
  -U flash:w:"Attiny85/attiny85-<версия>.hex":i
```

Фьюзы (ставятся один раз, при первой прошивке чипа): `E:FF, H:DF, L:62`.

## 4. ЕСП — обе части одной командой

```shell
~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 460800 \
  write_flash --flash_freq 40m --flash_size <4MB|1MB> --flash_mode qio \
  0x0 ESP8266/.pio/build/<окружение>/firmware.bin \
  <адрес ФС> ESP8266/.pio/build/<окружение>/littlefs.bin
```

**Двумя отдельными командами не получится.** После записи прошивки ЕСП
перезагружается и уходит в свой цикл i2c и сна, автосброс больше не роняет её в
загрузчик, и вторая запись умирает на `Failed to connect to ESP8266: Timed out
waiting for packet header`. Одна команда держит чип в загрузчике между записями.

По той же причине не годятся `pio run -t upload` и следом `-t uploadfs`.

## 5. Чем доказывается успех

Ровно двумя вещами:

```
avrdude: 6644 bytes of flash verified
...
Wrote  662592 bytes at 0x00000000 …  Hash of data verified.
Wrote 1024000 bytes at 0x00300000 …  Hash of data verified.
```

Всё, прошивка записана. Больше проверять нечего.

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
