# NodeMCU с AT-прошивкой: HTTP-клиент внутри сети Ватериуса

Третий компонент стенда, после METF и точки доступа. Нужен ровно для одного:
**оказаться внутри Wi-Fi сети, которую поднимает сам Ватериус в режиме
настройки**, и сходить по HTTP на его портал (`192.168.4.1`).

Ни один другой участник стенда этого не может. Рабочая машина сидит на
проводной стороне и своим единственным радио занята. Точка доступа стенда
(WT32-ETH01) в сборке с проводным аплинком поднимает радио как `WIFI_MODE_AP`
(`esp32_nat_router.c`, вызов `esp_wifi_set_mode`) - она умеет быть точкой, но
не умеет присоединяться к чужой. Ватериус же в режиме настройки - именно
чужая точка.

NodeMCU присоединяется к ней как обычный клиент, а управляется по USB. Своей
прошивки писать не пришлось: подходит официальная AT-прошивка Espressif.

## Что проверено на живой плате

| | |
|---|---|
| Прошивка | `AT version:2.3.0.0(2522d50 - ESP8266 - Aug 13 2025)` |
| Конфигурация | `Bin version:2.3.0(WROOM-02-N)` - консоль на GPIO1/GPIO3 |
| Связь | AT отвечает `OK` по штатному USB NodeMCU, без переходника |
| Команд | 100 (`AT+CMD?`) |
| Мусор при старте | нет: после сброса порт чистый, первая же `AT` отвечает |

## Почему не проще

**Не `AT+HTTPCLIENT`.** Соблазнительная команда - весь запрос одной строкой -
в готовых бинарниках ESP8266 **отсутствует**, и это не случайность: в
`sdkconfig` всех выпусков (2.2.0.0, 2.2.1.0, 2.2.2.0, 2.3.0.0) стоит
`# CONFIG_AT_HTTP_COMMAND_SUPPORT is not set`. Проверено и на живой плате:
в списке `AT+CMD?` её нет. Включается только пересборкой прошивки с
`ESP8266_RTOS_SDK` - ради чего мы и брали готовый бинарник.

Для нашей задачи она всё равно хуже: отдаёт тело ответа в порт целиком, а
файлы портала надо тянуть кусками и хешировать на лету - у ESP8266 мало
памяти. Это делают `AT+CIPRECVMODE` + `AT+CIPRECVDATA`, которые на месте.

**Не SLIP.** [`martin-ger/esp_slip_router`](https://github.com/martin-ger/esp_slip_router)
даёт ПК настоящий IP-маршрут через последовательный порт, и тогда Chrome
открывал бы портал напрямую. Но документирован только Linux (`slattach`), а в
macOS SLIP нет вовсе: ни `slattach`, ни kext (есть только PPP, а PPP-сервера у
этой прошивки нет). Для стенда на Mac это лишняя виртуалка.

## Прошивка

Берём [ESP-WROOM-02-AT-V2.3.0.0.zip](https://dl.espressif.com/esp-at/firmwares/esp8266/ESP-WROOM-02-AT-V2.3.0.0.zip)
(выпуск от 14.08.2025). Распакованное лежит в `~/CODE/esp8266_at/`.

```
sha256(esp-at.bin) = 8a37b3681cdb10cd58683fbf64ed04ef35b5613948133d018325465625d3a5e3
```

### Выводы консоли - главная засада

По умолчанию AT-прошивка ESP8266 говорит на **GPIO15/GPIO13**, а USB-мост
NodeMCU припаян к **GPIO1/GPIO3**. Прошьёте как есть - плата работает, а порт
молчит.

Лечится не пересборкой, а параметром: у Espressif есть готовая строка модуля
именно под эту распайку - в
[`factory_param_data.csv`](https://github.com/espressif/esp-at/blob/release/v2.3.0.0_esp8266/components/customized_partitions/raw_data/factory_param/factory_param_data.csv):

```
PLATFORM_ESP8266,WROOM-02-N,TX:1 RX:3,...,115200,1,3,-1,-1,-1,-1
```

`-1` в полях `cts`, `rts` и управляющих выводов означает «выключено» - заодно
снимается аппаратное управление потоком (в сборке по умолчанию
`CONFIG_AT_UART_DEFAULT_FLOW_CONTROL=1`) и не дёргается GPIO5, который у
варианта по умолчанию задан как `tx_control_pin`.

Бинарник параметров сгенерирован штатным
[`tools/factory_param_generate.py`](https://github.com/espressif/esp-at/blob/release/v2.3.0.0_esp8266/tools/factory_param_generate.py)
(нужен `pip install xlrd`):

```bash
python3 factory_param_generate.py \
  --platform PLATFORM_ESP8266 --module WROOM-02-N \
  --module_file factory_param_data.csv --define_file factory_param_type.csv \
  --bin_name out/factory_param_NODEMCU.bin --log_file out/fp.log
```

Результат - `01 03 ff ff ff ff` по смещению `0x10`, то есть TX=1, RX=3,
остальное выключено:

```
sha256(factory_param_NODEMCU.bin) = 33f01c5dc7ac2516ce1e490e05b4f527bba1c1273e88e691036a4c539475a0d1
```

### Запись

Адреса - из `download.config` самого пакета. Разделы с сертификатами MQTT не
пишем: MQTT в этой сборке вкомпилирован Espressif, но мы им не пользуемся.

```bash
cd ~/CODE/esp8266_at
esptool --chip esp8266 -p /dev/cu.usbserial-XXXX --before default_reset --after hard-reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 2MB \
  0x0     bootloader/bootloader.bin \
  0x8000  partition_table/partition-table.bin \
  0x9000  ota_data_initial.bin \
  0x10000 esp-at.bin \
  0xF0000 at_customize.bin \
  0xF1000 factory_param_NODEMCU.bin
```

Разметка на 2 МБ; у NodeMCU флеш 4 МБ, остаток просто не используется.
Стирать перед записью - `erase_flash` теми же ключами.

**`--before default_reset`, а не `no-reset`.** У NodeMCU есть схема
автосброса, и она работает; попытка сэкономить на сбросе даёт
`Failed to connect to ESP8266: No serial data received`.

**Признак успеха:** `Hash of data verified.` шесть раз.

## Проверка после прошивки

```bash
python3 -m serial.tools.miniterm /dev/cu.usbserial-XXXX 115200
```

Отправить `AT` - ответ `OK`. `AT+GMR` печатает версию; в строке
`Bin version` должно стоять **`WROOM-02-N`** - это доказательство, что
параметры выводов применились.

## Команды

Проверено на плате: в списке есть всё нужное.

| Команда | Зачем |
|---|---|
| `AT`, `AT+GMR`, `AT+RST` | связь, версия, сброс |
| `AT+CMD?` | список поддерживаемых команд |
| `AT+CWMODE=1` | режим клиента |
| `AT+CWJAP="<ssid>","<pass>"` | присоединиться к точке Ватериуса |
| `AT+CIFSR` | свой адрес |
| `AT+CIPMUX=0` | одно соединение |
| `AT+CIPSTART="TCP","192.168.4.1",80` | соединение с порталом |
| `AT+CIPSEND=<n>` | отправить `n` байт: строку запроса HTTP |
| `AT+CIPRECVMODE=1` | пассивное чтение: данные не сыплются в порт сами |
| `AT+CIPRECVLEN?` | сколько байт ждёт |
| `AT+CIPRECVDATA=<n>` | забрать `n` байт |
| `AT+CIPCLOSE` | закрыть соединение |
| `AT+UART_CUR=921600,8,1,0,0` | поднять скорость порта |

Пассивное чтение - не украшение. Без него ответ валится в порт как `+IPD`, и
на `strings.js` в 24 КБ буфер переполняется.

**Скорость.** Портал - 208 КБ в 35 файлах. На 115200 это ~18 секунд на весь
набор, на 921600 - около трёх.

## Практические мелочи драйвера

- **`AT+CMD?` читать до `OK`, а не «сколько успело прийти».** Ответ длинный:
  чтение по таймауту в 4 секунды даёт 35 строк из 100, и список выглядит
  обрезанным, будто команд нет.
- Перед первой командой сбрасывать входной буфер.
- Ответы AT заканчиваются `OK` или `ERROR`; ждать надо их, а не паузу.

## Чего ещё не проверяли

`AT+CWJAP` и связка `AT+CIPSTART`/`AT+CIPSEND`/`AT+CIPRECVDATA` на этой плате
пока **не запускались** - только подтверждено, что команды поддерживаются.
Проверять их надо на Ватериусе в режиме настройки (кнопка дольше 4 с, стенд
умеет это `dut.hold_button()`).

## Что на этой плате будет проверяться

**Обход всех файлов портала.** Плата запрашивает каждый путь и отдаёт в порт
код ответа, размер и хеш; сверяем с `ESP8266/data/` - это те же байты, что
уходят в образ LittleFS. Проверка сильнее браузерной: браузер грузит только
то, на что ссылается страница, а обход по списку ловит файл, который забыли
положить в образ или который в него не влез.

**Настоящий API портала.** `/api/*` - это JSON, он мал и проходит по serial
мгновенно. Проверяется то, чего не покрывает симулятор: что прошивка
принимает настройки, валидирует их и сохраняет.

Отрисовку страниц сюда не тащим: её проверяет
[симулятор](../../simulator/README.md) на локально поднятых файлах, и правило
репозитория требует править его тем же PR, что и портал.

## Источники

- [ESP-AT, выпуски для ESP8266](https://github.com/espressif/esp-at/releases) - ветка `release/v2.3.0.0_esp8266`
- [Бинарник ESP-WROOM-02-AT-V2.3.0.0](https://dl.espressif.com/esp-at/firmwares/esp8266/ESP-WROOM-02-AT-V2.3.0.0.zip)
- [HTTP AT Commands (ESP8266)](https://espressif-docs.readthedocs-hosted.com/projects/esp-at/en/release-v2.2.0.0_esp8266/AT_Command_Set/HTTP_AT_Commands.html) - там же оговорка, что в готовых сборках команд может не быть
- [`factory_param_data.csv`](https://github.com/espressif/esp-at/blob/release/v2.3.0.0_esp8266/components/customized_partitions/raw_data/factory_param/factory_param_data.csv) - строка `WROOM-02-N`
- [`tools/factory_param_generate.py`](https://github.com/espressif/esp-at/blob/release/v2.3.0.0_esp8266/tools/factory_param_generate.py)
- [`martin-ger/esp_slip_router`](https://github.com/martin-ger/esp_slip_router) - отвергнутый вариант с настоящим IP-маршрутом
