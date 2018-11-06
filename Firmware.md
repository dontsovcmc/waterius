# Quickstart
Инструкция по заливке готовых прошивок без компиляции и IDE для Windows

#### Прошивка attiny85
1. Скачивем Avrdude: http://www.avislab.com/blog/wp-content/uploads/2012/12/avrdude.zip
2. Распаковываем архив, заходим в папку. Открываем консоль: shift+правкая кнопка мыши - Открыть окно команд
3. Скачивем прошивку attiny85:
`curl https://raw.githubusercontent.com/dontsovcmc/waterius_firmware/master/0.5/attiny85.hex --output ./attiny85.hex`
Если нет curl, то открываем ссылку и копируем файл в папку Avrdude.
4. Ставим драйвер программатора [USBAsp](http://www.myrobot.ru/downloads/driver-usbasp-v-2.0-usb-isp-windows-7-8-10-xp.php) и подключаем его с attiny85.
5. `avrdude.exe -p t85 -c Usbasp -B 4 -P usb  -U efuse:w:0xFF:m -U hfuse:w:0xDF:m -U lfuse:w:0x62:m`
6. `avrdude.exe -p t85 -c Usbasp -B 4 -P usb -U flash:w:"attiny85.hex":a`

#### Прошивка ESP8266
1. Ставим Питон 2.7, добавляем в PATH
2. pip install esptool
3. Скачивем прошивку ESP8266: `curl https://raw.githubusercontent.com/dontsovcmc/waterius_firmware/master/0.5/esp8266.bin --output ./esp8266.bin`
Если нет curl, то открываем ссылку и заходим в папку с файлом.
4. Подключаем USB-TTL с ESP8266
5. `python -m esptool --baud 115200 --port COM7 write_flash --flash_freq 40m --flash_size 1MB --flash_mode dio --verify 0x0 esp8266.bin`
COM7 замените на порт USB-TTL

# Прошивка Attiny85

#### Готовые прошивки: [waterius_firmware](https://github.com/dontsovcmc/waterius_firmware)

Фьюзы: E:FF, H:DF, L:62

Расположение выводов на разъеме для ESP-01 (вид сверху):

| **GND** | **SCK 15** | **MOSI 16** | nc  | 
| ---- | ---- | ---- | ---- |
|  nc | **MISO 14** | nc  | **Vcc** |
+ 10й пин на ресет Attiny85

nc - не используется
Vcc - в любой 3.3в или 5в.

## Arduino в качестве ISP программатора (3.3в-5в).

Распиновка, при прошивки с помощью Arduino Micro или Arduino UNO:

| Micro | UNO | ISP | Attiny85 |   
| ---- | ---- | ---- | ---- |
| 15pin | 13pin | SCK | 7pin |
| 14pin | 12pin | MISO | 6pin |
| 16pin | 11pin | MOSI | 5pin |
| 10pin | 10pin | RESET | 1pin |
+ питание!

В platfomio.ini:
upload_protocol = arduino
upload_flags = -P$UPLOAD_PORT
upload_speed = 19200

## Китайский USB-ISP программатор
Плата MX-USBISP-V5.00
Программа [ProgISP V1.7.2](https://yandex.ru/search/?text=ProgISP%20V1.7.2&&lr=213)
Фьюзы: E:FF, H:DF, L:62
HEX файл взять из platfomio или Arduino IDE.

## USBasp программатор
Я купил китайский USB-ISP и перепрошил его по [инструкции](https://vochupin.blogspot.com/2016/12/usb-isp.html) в USBasp ([прошивка](https://www.fischl.de/usbasp/)). В диспетчере устройств он стал виден, как USBasp. 
Драйвер [v3.0.7](http://www.myrobot.ru/downloads/programs/usbasp-win-driver-x86-x64-v3.0.7.zip)
В platfomio.ini:
upload_protocol = usbasp
upload_flags = 
    -Pusb 
    -B5

Примечание: в Windows7 почему-то не заработал. Windows 10x64 - ок.
	
## Avrdude_prog
```
avrdude.exe -p t85 -c Usbasp -B 4 -P usb  -U efuse:w:0xFF:m -U hfuse:w:0xDF:m -U lfuse:w:0x62:m
avrdude.exe -p t85 -c Usbasp -B 4 -P usb -U flash:w:"<путь_до_репозитория>\waterius\Attiny85\.pioenvs\attiny85\firmware.hex":a
```

## Работа с platfomio
Platformio бывает в виде консольной утилиты или как дополнение в Visual Studio Code. 
[Инструкция по установки утилиты](http://docs.platformio.org/en/latest/installation.html#python-package-manager)


[Инструкция из интернета](https://medium.com/jungletronics/attiny85-easy-flashing-through-arduino-b5f896c48189) 
У нас только attiny85 уже сидит на плате, поэтому подключаемся к разъему.

### c помощью platfomio
- откройте в командной строке папку waterius/Attiny85
- измените в файле platfomio.ini порт на свой:
upload_port = /dev/tty.usbmodem1421
- выполните:
platformio run --target upload

Используемые библиотеки:
* [WiFiManager](https://github.com/tzapu/WiFiManager) для настройки wi-fi точки доступа
* [USIWire](https://github.com/puuu/USIWire) i2c слейв для attiny
    

# Прошивка ESP8266
Для прошивки ESP8266 необходим USB-TTL преобразователь с логическим уровнем 3.3в. Обратите внимание, что у него должен быть регулятор напряжения для питания ESP8266 на 3.3в. У обычных USB-TTL преобразователей логический уровень 5в, поэтому их вывод TX нужно подключить к делителю напряжения. Я использую резисторы 1.5к и 2.2к.

[Инструкция из интернета](http://cordobo.com/2300-flash-esp8266-01-with-arduino-uno) 
(в большинстве других туториалах подключают 5в логику и делают ESP больно)

[Скомпилированные прошивки](https://github.com/dontsovcmc/waterius_firmware)

### c помощью platfomio
- откройте в командной строке папку waterius/ESP8266
- измените в файле platfomio.ini порт на свой:
upload_port = /dev/tty.usbmodem1411
- выполните:
platformio run --target upload

### с помощью esptool
`$ python -m esptool --baud 115200 --port COM7 write_flash --flash_freq 40m --flash_size 1MB --flash_mode dio --verify 0x0 esp.bin`

<details>
 <summary>output log</summary>
	
```
esptool.py v2.5.0
Serial port COM7
Connecting........_____.....____
Detecting chip type... ESP8266
Chip is ESP8266EX
Features: WiFi
MAC: 68:c6:3a:a4:75:b0
Uploading stub...
Running stub...
Stub running...
Configuring flash size...
Flash params set to 0x0220
Compressed 359840 bytes to 253754...
Wrote 359840 bytes (253754 compressed) at 0x00000000 in 23.1 seconds (effective 124.8 kbit/s)...
Hash of data verified.

Leaving...
Verifying just-written flash...
(This option is deprecated, flash contents are now always read back after flashing.)
Flash params set to 0x0220
Verifying 0x57da0 (359840) bytes @ 0x00000000 in flash against esp.bin...
-- verify OK (digest matched)
Hard resetting via RTS pin...
```
</details>

### с помощью Arduino IDE
#### Additional Libraries Требуемые библиотеки  
- WifiManager by tzapu (0.12.0)
- Blynk by Volodymyr Shymanskyy (0.5.2)

(!) Patch WifiManager library: 
1. Open WiFiManager.h file
2. Move WiFiManagerParameter class 'init' function from private to public 


#### Additional Boards Managers URLs:
http://arduino.esp8266.com/stable/package_esp8266com_index.json

Board settings:
* Board: Generic ESP8266 Module
* Flash Mode: DIO
* Flash Size: 512K (no SPIFFS)
* Debug port: Disable
* Debug Level: None
* IwIP Varian: v2 Lover Memory
* Reset Method: ck
* Crystal Frequency: 26 MHz
* Flash Frequency: 40MHz
* CPU Frequency: 80 MHz
* Buildin Led: 0
* Upload Speed: 115200
* Port: select your port

#### Sketch
1. rename main.cpp to src.ino 
2. open src.ino in Arduino IDE
3. compile









