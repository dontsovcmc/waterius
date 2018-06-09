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

| **GND** | **SCK 15** | **MOSI 16** | nc  | 
| ---- | ---- | ---- | ---- |
|  nc | **MISO 14** | nc  | **Vcc** |
+ 10й пин на ресет Attiny85

nc - не используется
Vcc - в любой 3.3в или 5в.

Используемые библиотеки:
* [WiFiManager](https://github.com/tzapu/WiFiManager) для настройки wi-fi точки доступа
* [USIWire](https://github.com/puuu/USIWire) i2c слейв для attiny


### c помощью platfomio
- откройте в командной строке папку ImpCounter85
- измените в файле platfomio.ini порт на свой:
upload_port = /dev/tty.usbmodem1421
- выполните:
platformio run --target upload

## Прошивка ESP8266
- скачайте библиотеку [WiFiManager](https://github.com/tzapu/WiFiManager)
- откройте в командной строке папку ImpCounterESP
- измените в файле platfomio.ini порт на свой:
upload_port = /dev/tty.usbmodem1411
- выполните:
platformio run --target upload_port

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
