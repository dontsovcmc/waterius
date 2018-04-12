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
