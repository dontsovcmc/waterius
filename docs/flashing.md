# Прошивка платы по проводам

Готовый порядок действий: определить плату, собрать, записать, убедиться. Всё,
что ниже, проверено на живой плате — команды можно копировать как есть.

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

## 1. Порт и плата

Портов в списке много, нужен тот, за которым стоит USB-UART:

```shell
ls /dev/cu.*
system_profiler SPUSBDataType | grep -B2 -A6 "FT232\|CP210"
```

Плата определяется размером флеша, а не на глаз:

```shell
~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 115200 flash_id
```

| Detected flash size | Плата | Раздел |
|---|---|---|
| 4MB | Ватериус-2 (ESP-12F) | [2](#2-ватериус-2) |
| 1MB | Классик (ESP-01) | [3](#3-классик) |

Дальше идите в свой раздел и не смешивайте команды из соседнего.

## 2. Ватериус-2

Две прошивки, attiny первой: ЕСП при старте читает у attiny версию и данные.

### 2.1. attiny

1. **Папка:** `-d Attiny85`
2. **Окружение:** `-e waterius_2`
3. **Файл на выходе:** `Attiny85/waterius_2-43.hex` — имя собирает
   `Attiny85/objdump.py` из окружения и `firmware_version`
   (`Attiny85/platformio.ini`); с `LOG_ON` добавляется `-log`

```shell
~/.platformio/penv/bin/pio run -d Attiny85 -e waterius_2

~/.platformio/packages/tool-avrdude/bin/avrdude \
  -C ~/.platformio/packages/tool-avrdude/avrdude.conf \
  -p t85 -c usbasp -B 4 -P usb \
  -U flash:w:"Attiny85/waterius_2-43.hex":i
```

### 2.2. ЕСП

1. **Папка:** `-d ESP8266`
2. **Окружение:** `-e waterius_2`
3. **Файлы на выходе:** `ESP8266/waterius_2-2.0.50.bin` и
   `ESP8266/waterius_2-2.0.50-fs.bin` — имена собирает
   `ESP8266/post_compile.py` из окружения и `firmware_version`
   (`ESP8266/platformio.ini`), у образа ФС суффикс `-fs`; если в
   `ESP8266/secrets.ini` задан `WIFI_SSID`, добавляется ещё `-test`

```shell
~/.platformio/penv/bin/pio run -d ESP8266 -e waterius_2
~/.platformio/penv/bin/pio run -d ESP8266 -e waterius_2 -t buildfs

~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 460800 \
  write_flash --flash_freq 40m --flash_size 4MB --flash_mode qio \
  0x0      ESP8266/waterius_2-2.0.50.bin \
  0x300000 ESP8266/waterius_2-2.0.50-fs.bin
```

## 3. Классик

Порядок тот же: attiny первой, потом ЕСП.

### 3.1. attiny

1. **Папка:** `-d Attiny85`
2. **Окружение:** `-e attiny85`
3. **Файл на выходе:** `Attiny85/attiny85-43.hex`

```shell
~/.platformio/penv/bin/pio run -d Attiny85 -e attiny85

~/.platformio/packages/tool-avrdude/bin/avrdude \
  -C ~/.platformio/packages/tool-avrdude/avrdude.conf \
  -p t85 -c usbasp -B 4 -P usb \
  -U flash:w:"Attiny85/attiny85-43.hex":i
```

### 3.2. ЕСП

1. **Папка:** `-d ESP8266`
2. **Окружение:** `-e esp01_1m`
3. **Файлы на выходе:** `ESP8266/esp01_1m-2.0.50.bin` и
   `ESP8266/esp01_1m-2.0.50-fs.bin`

```shell
~/.platformio/penv/bin/pio run -d ESP8266 -e esp01_1m
~/.platformio/penv/bin/pio run -d ESP8266 -e esp01_1m -t buildfs

~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 460800 \
  write_flash --flash_freq 40m --flash_size 1MB --flash_mode qio \
  0x0     ESP8266/esp01_1m-2.0.50.bin \
  0xBB000 ESP8266/esp01_1m-2.0.50-fs.bin
```

## 4. Окружение обязано стоять в каждой команде

`-e` не для красоты: **окружение — это и есть выбор платы**, обе прошивки
различаются на этапе компиляции.

У ЕСП от него зависит линия i2c — GPIO0/GPIO2 против GPIO4/GPIO5
(`ESP8266/src/master_i2c.h`), светодиоды и кнопка (`ESP8266/src/setup.h`).
У attiny — вещи, от которых зависит, оживёт ли плата вообще:

| | Классик (`attiny85`) | Ватериус-2 (`waterius_2`) |
|---|---|---|
| Включение питания ЕСП | HIGH | **LOW** (`Attiny85/src/Power.cpp`, ESPPowerPin::power) |
| Кнопка | PB2 | **PB1** (`Attiny85/src/main.cpp`) |
| Пороги компаратора входов | под 3к3+300 Ом | под 1 кОм (`Attiny85/src/counter.h`) |

**Команда без `-e` не отказывается работать — она молча собирает классик.**
В `default_envs` у обоих проектов стоит он (`Attiny85/platformio.ini`,
`ESP8266/platformio.ini`), так что `pio run -d Attiny85` даёт
`attiny85-43.hex` и на Ватериусе-2 это мёртвая плата. Как она при этом
выглядит — в разборе неисправностей.

Версии в именах файлов (`43`, `2.0.50`) берутся из `firmware_version` в
`platformio.ini` каждого проекта. Подняли версию — имя файла поменялось,
сверьтесь с выводом сборки: он печатает путь, куда положил образ.

## 5. Почему ЕСП пишется одной командой

**Двумя отдельными командами не получится.** После записи прошивки ЕСП
перезагружается и уходит в свой цикл i2c и сна, автосброс больше не роняет её в
загрузчик, и вторая запись умирает на `Failed to connect to ESP8266: Timed out
waiting for packet header`. Одна команда держит чип в загрузчике между записями.

По той же причине не годятся `pio run -t upload` и следом `-t uploadfs`.

Адрес ФС, если понадобится считать самому, берётся из `board_build.ldscript` в
`ESP8266/platformio.ini`: `_FS_start` минус `0x40200000`.

## 6. Фьюзы и EEPROM attiny

Фьюзы ставятся один раз, при первой прошивке чипа: `E:FF, H:DF, L:62`.

EESAVE в них не запрограммирован, поэтому **каждая прошивка attiny стирает её
EEPROM**: счётчики импульсов начинаются с нуля. Показания счётчиков воды это не
затрагивает — они в настройках ЕСП.

## 7. Чем доказывается успех

Ровно двумя вещами:

```
avrdude: NNNN bytes of flash verified
...
Wrote  NNNNNN bytes at 0x00000000 …  Hash of data verified.
Wrote  NNNNNN bytes at <адрес ФС> …  Hash of data verified.
```

Всё, прошивка записана. Больше проверять нечего.

**«Verified» доказывает запись, а не выбор файла.** Прошивка не той платы
пишется и сверяется так же успешно, как своя: avrdude сверяет чип с тем файлом,
который ему дали, и о том, что файл чужой, не скажет никогда. Защищает от этого
только одно — окружение в имени файла (`waterius_2-43.hex` против
`attiny85-43.hex`). Поэтому раздел берётся целиком, а не собирается из кусков.

**Слушать UART после прошивки бессмысленно.** Питанием ЕСП распоряжается attiny
и между сеансами держит его выключенным, а сброс по RTS сеанса не заказывает —
порт будет молчать и на исправной плате. Хотите увидеть лог — это отдельное
действие: нажать кнопку на корпусе и только тогда читать порт на 115200.

## Если не подключается

| Симптом | Причина |
|---|---|
| `Failed to connect to ESP8266` | плата не в загрузчике: у Ватериуса-2 ЕСП запитывает attiny, дайте питание и держите GPIO0 на земле |
| `can't open config file` у avrdude | забыт `-C`, см. путь выше. У пакета из репозитория дистрибутива конфиг штатный, и `-C` не нужен вовсе |
| `command not found: avrdude` | берите из пакетов PlatformIO, в PATH его нет |
| `Permission denied` на порт (Linux) | пользователя нет в группе `dialout` (в Arch и Fedora - `uucp`); после `usermod -aG` нужен перелогин |
| `could not find USB device` у avrdude (Linux) | нет правила udev на USBasp: запускайте через `sudo` либо заведите правило по его идентификаторам из `lsusb` |
| Запись срывается на 460800 | понизьте до `--baud 115200`: не всякий USB-UART держит эту скорость на длинном проводе |
| `Detected flash size` не совпал с ожидаемой платой | плата другая: раздел, окружение и адрес ФС тоже другие |
| Нет файла `<окружение>-<версия>.hex`/`.bin` | сборка шла с другим `-e`, и образ лежит под другим именем |
| После прошивки: удержание кнопки ничем не подсвечивается, затем 5 вспышек красного | ЕСП не получила ответа attiny по i2c. `main.cpp:loop` пропускает весь блок, `config_loaded` остаётся false, `blynk_error(ERROR_CONFIG=5)`. Первое, что проверить, — не залита ли в attiny прошивка чужой платы |
