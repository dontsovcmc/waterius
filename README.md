# Wi-Fi счётчик импульсов 
## (проект в стадии разработки)
Автономный счётчик импульсов с питанием от батареек АА 2шт или 3.3В. Данные передаются по Wi-Fi в Телеграм-бота https://web.telegram.org/#/im?p=@ZKHBot.

<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" width="400"/> <img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" width="400"/>

Можно подключить два счетчика. 
Можно настроить период измерения и период отправки измерений на сервер.

Потребление
* в режиме сна: 25 мкА (40мкА при замкнутом входе)
* в режиме передачи данных: 70мА (~2 секунды)

Двух батареек АА должно хватить на несколько лет!

## Принцип работы
Счётчик импульсов состоит из 2-х микросхем. Attiny85 считает импульсы в режиме сна. Раз в Х минут  просыпается и сохраняет текущее значение в буфер. Раз в Y минут при очередном просыпании она будит ESP8266 импульсом на Reset и слушает i2c линию. Проснувшись, ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого все микросхемы засыпают.

## Установка счётчика
- получите ID и пароль для счетчика у телеграм бота по команде /newid (выполняется только для указанного при запуске администратора)
- подключите счётчики воды к разъемам Wi-fi счётчика
- включите питание
- нажмите кнопки Reset + Setup
- отпустите кнопку Reset
- отпустите кнопку Setup. ESP8266 войдет в режим настройки.
- найдите телефоном Wi-Fi точку доступа ImpulsCounter_X
- откройте http://192.168.4.1
- введите имя и пароль от Wi-Fi, статический ip Wi-fi счётчика, ID и пароль к серверу
- нажмите ОК
- переподключите питание.

## Компоненты
* однослойная плата 50 х 18 мм
* Attiny85
* ESP8266-01 (или другая модификация)
* резистор 0.125вт 3к3-3к6
* резистор SMD 0805 3к3-3к6
* резистор 0.125вт 47к
* резистор SMD 0805 47к
* резистор SMD 0805 10к
* конденсатор ~0.1мкФ - 3шт
* конденсатор ~10мкФ 
* разъем 4х2пин для ESP-01
* разъем Wago 235-102 - 2шт (или аналог)
* разъем 1пин мама для прошивки Attiny85
* кнопка 5мм - 2шт
* (разъем 1пин для отладки)

+ USB-TTL 3.3в для прошивки ESP8266
+ плата размером 

## Изготовление
- создание платы ([1 слойный вариант под ESP-01](https://github.com/dontsovcmc/ImpCounter/blob/master/Board/board-ESP-01-1layer.png))
- прошивка Attiny85 
- прошивка ESP 
- установите ESP на плате
- подключение питания 

## Прошивка Attiny85

Прошивка ISP программатором (3.3в-5в).

Распиновка, при прошивки с помощью Arduino Micro или Arduino UNO:

| Micro | UNO | ISP | Attiny85 |   
| ---- | ---- | ---- | ---- |
| 15pin | 13pin | SCK | 7pin |
| 14pin | 12pin | MISO | 6pin |
| 16pin | 11pin | MOSI | 5pin |
| 10pin | 10pin | RESET | 1pin |

Расположение выводов на разъеме для ESP-01 (вид сверху):

| **GND** | **SCK** | **MOSI** | nc  | 
| ---- | ---- | ---- | ---- |
|  nc | **Vcc** | **MISO** | **Vcc** |

nc - не используется
Vcc - в любой 3.3в или 5в.

Используемые библиотеки:
* [WiFiManager](https://github.com/tzapu/WiFiManager) для настройки wi-fi точки доступа
* [EdgeDebounceLite](https://github.com/j-bellavance/EdgeDebounceLite) для устранения дребезга контактов счетчика
* [USIWire](https://github.com/puuu/USIWire) i2c слейв для attiny. с исправленной ошибкой [#5](https://github.com/puuu/USIWire/issues/5)


### c помощью platfomio
- откройте в командной строке папку ImpCounter85
- измените в файле platfomio.ini порт на свой:
upload_port = /dev/tty.usbmodem1421
- выполните:
platformio run --target upload

## Прошивка ESP8266
- скачайте библиотеку [WiFiManager](https://github.com/tzapu/WiFiManager)
- измените в файле Setup.h:
'''Раскомментируйте
//#define WEMOS
#define ESP_01
//#define NODEMCU
в зависимости от модификации ESP'''

### c помощью platfomio
- откройте в командной строке папку ImpCounterESP


- измените в файле platfomio.ini:
порт на свой:
upload_port = /dev/tty.usbmodem1421
измените окружение в зависимости от модификации ESP:
для ESP-01:
[env:esp01_1m]
board = esp01_1m

- выполните:
platformio run --target upload

## Запуск Telegram бота
Чтобы счетчик присылал вам показания, нужно запустить TCP сервер с телеграм ботом.
в файл key запишите ключ бота, полученный от BotFather
python main.py "$(< key)" --shost 127.0.0.1 --sport 4001 --host 127.0.0.1 --port 443 --admin your_nickname
shost - адрес TCP сервера
sport - порт TCP сервера, по умолчанию 4001 
host, port - адрес, порт Телеграм бота. Соединение по webhook (требует сертификаты). Если не указать, то будет long polling.
admin - ваш ник для генерации ID, пароля счетчиков на вашем сервере.

# Благодарности
Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1). именно он был взят за основу и добавлены прерывания


