# Quickstart
Как можно прошить attiny & esp:
1. Взять готовые hex файлы и залить их
2. Скомпилировать исходный код в platformio (cli или visual studio code)
3. Скомпилировать исходный код в Arduino IDE

Чем прошить attiny:
- Программатором USBAsp
- Платой Arduino, загрузив в нее скетч Arduino-as-ISP
- другим программатором для AVR

Чем прошить ESP8266:
- USB-TTL 3.3v переходником
- Платой Arduino, подключившись к ee RX,TX + делитель до 3.3v


## Программаторы Attiny
Фьюзы: E:FF, H:DF, L:62

#### Утилита Avrdude
http://www.avislab.com/blog/wp-content/uploads/2012/12/avrdude.zip

#### Arduino в качестве ISP программатора (3.3в-5в).

1. Залейте скетч ISP программатора с помощью Arduino IDE в плату Arduino [[инструкция](http://www.martyncurrey.com/arduino-nano-as-an-isp-programmer/)]
2. Подключите плату Arduino к Вотериусу:
Распиновка, при прошивки с помощью Arduino Micro или Arduino UNO:

| Micro | UNO/NANO | ISP | Attiny85 |   
| ---- | ---- | ---- | ---- |
| 15pin | 13pin | SCK | 7pin |
| 14pin | 12pin | MISO | 6pin |
| 16pin | 11pin | MOSI | 5pin |
| 10pin | 10pin | RESET | 1pin |
+ питание!

В platfomio.ini:
```
upload_protocol = arduino
upload_flags = -P$UPLOAD_PORT
upload_speed = 19200
```

#### Распиновка разъема Ватериус для прошивки attiny
(вид сверху)

| **GND** | **SCK 15** | **MOSI 16** | nc  | 
| ---- | ---- | ---- | ---- |
|  nc | **MISO 14** | nc  | **Vcc** |
+ 10й пин на ресет Attiny85

nc - не используется
Vcc - в любой 3.3в или 5в.

#### Китайский USB-ISP программатор
Плата MX-USBISP-V5.00
Программа [ProgISP V1.7.2](https://yandex.ru/search/?text=ProgISP%20V1.7.2&&lr=213)
Фьюзы: E:FF, H:DF, L:62

#### USBasp программатор
Я купил китайский USB-ISP и перепрошил его по [инструкции](https://vochupin.blogspot.com/2016/12/usb-isp.html) в USBasp ([прошивка](https://www.fischl.de/usbasp/)). В диспетчере устройств он стал виден, как USBasp. 
Драйвер [v3.0.7](http://www.myrobot.ru/downloads/programs/usbasp-win-driver-x86-x64-v3.0.7.zip)
В platfomio.ini:
```
upload_protocol = usbasp
upload_flags = 
    -Pusb 
    -B5
```
Примечание: в Windows7 почему-то не заработал. Windows 10x64 - ок.
	
## Программаторы ESP8266
Для прошивки ESP8266 необходим USB-TTL преобразователь с логическим уровнем 3.3в. Обратите внимание, что у него должен быть регулятор напряжения для питания ESP8266 на 3.3в. У обычных USB-TTL преобразователей логический уровень 5в, поэтому их вывод TX нужно подключить к делителю напряжения. Я использую резисторы 1.5к и 2.2к.

[Инструкция из интернета](http://cordobo.com/2300-flash-esp8266-01-with-arduino-uno) 
(в большинстве других туториалах подключают 5в логику и делают ESP больно)

#### Драйверы для USB-TTL:
- [CH430G](https://all-arduino.ru/drajver-ch340g-dlya-arduino/)
- [PL2303](http://www.prolific.com.tw/US/ShowProduct.aspx?p_id=225&pcid=41)

## Готовые hex файлы
[На странице releases](https://github.com/dontsovcmc/waterius/releases)

### Прошивка attiny85 с помощью Avrdude & Usbasp
1. Скачивем Avrdude: http://download.savannah.gnu.org/releases/avrdude/avrdude-6.2-mingw32.zip
2. Распаковываем архив, заходим в папку. Открываем консоль: shift+правкая кнопка мыши - Открыть окно команд
3. Скачивем прошивку attiny85:
`curl https://raw.githubusercontent.com/dontsovcmc/waterius_firmware/master/0.5/attiny85.hex --output ./attiny85.hex`
Если нет curl, то открываем ссылку и копируем файл в папку Avrdude.
4. Ставим драйвер программатора [USBAsp](http://www.myrobot.ru/downloads/driver-usbasp-v-2.0-usb-isp-windows-7-8-10-xp.php) и подключаем его с attiny85.
5. `avrdude.exe -p t85 -c Usbasp -B 4 -P usb  -U efuse:w:0xFF:m -U hfuse:w:0xDF:m -U lfuse:w:0x62:m`
6. `avrdude.exe -p t85 -c Usbasp -B 4 -P usb -U flash:w:"D:\firmware.hex":a`, ге D:\firmware.hex - путь до файла. У вас свой.


### Прошивка ESP8266
Программатор не нужен, а нужен переходник с USB на TTL 3.3 вольт.

1. Ставим Питон 3.8 (2.7 тоже подойдет, только пути для PATH другие).
Win: Добавляем в PATH:
C:\Users\Админ\AppData\Local\Programs\Python\Python38-32
2. Скачиваем pip. Выполняем python get-pip.py - Питон установит утилиту pip.
добавляем в PATH:
C:\Users\Админ\AppData\Local\Programs\Python\Python38-32\Scripts
2. pip install esptool
3. Скачивем [прошивку ESP8266](https://github.com/dontsovcmc/waterius/releases) файл esp8266.bin
4. Подключаем USB-TTL с ESP8266 замкнув GPIO0 на GND
5. `python -m esptool --port COM7--baud 115200 write_flash --flash_freq 40m --flash_size 1MB --flash_mode qio 0x0 esp8266-1.0.2.bin 0xbb000 esp8266-1.0.2-fs.bin`

COM7 замените на свой порт

<details>
 <summary>output log (esptool 2.5.0)</summary>
	
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

Очистить конфигурацию можно вместе с памятью:
python -m esptool --chip esp8266 --port COM3 --after no_reset erase_flash


## Прошивка через PlatformIO
### Установка PlatformIO
PlatformIO бывает в виде консольной утилиты или как дополнение в Visual Studio Code. 
[Инструкция по установки утилиты](http://docs.platformio.org/en/latest/installation.html#python-package-manager)

[Инструкция из интернета](https://medium.com/jungletronics/attiny85-easy-flashing-through-arduino-b5f896c48189) 
У нас только attiny85 уже сидит на плате, поэтому подключаемся к разъему.

После установки в командной строке можно вызывать `platformio --version` и увидеть версию platformio

### Прошивка Attiny
- откройте в командной строке папку waterius/Attiny85
- измените в файле platfomio.ini порт на свой:
upload_port = /dev/tty.usbmodem1421
- выполните:
platformio run --target upload


### Прошивка ESP8266
- откройте в командной строке папку waterius/ESP8266
- измените в файле platfomio.ini порт на свой:
upload_port = /dev/tty.usbmodem1411
- прошейте сначала файл прошивки:
platformio run --target upload --environment esp01_1m
- затем прошейте файловую систему:
platformio run --target uploadfs --environment esp01_1m


## Прошивка с помощью Arduino IDE

### Attiny

Установить [поддержку Attiny плат](https://github.com/SpenceKonde/ATTinyCore/blob/master/Installation.md)
0. выбрать 1 MHz internal 

1. В /attiny/scr переименовать main.cpp в src.ino 
2. Открыть src.ino в Arduino IDE
3. Компилировать

### ESP8266
#### Additional Libraries Требуемые библиотеки   

* Blynk by Volodymyr Shymanskyy (0.6.1)
* ArduinoJSON (6.12.0)
* PubSubClient (2.7.0)
* Установить вручную из zip [WiFiManager#waterius_release_101](https://github.com/dontsovcmc/WiFiManager/tree/waterius_release_101) 
Примечание: актуальные версии в [platformio.ini](https://github.com/dontsovcmc/waterius/blob/master/ESP8266/platformio.ini)

#### ESP8266: Additional Boards Managers URLs:
http://arduino.esp8266.com/stable/package_esp8266com_index.json

Board settings:
* Board: Generic ESP8266 Module
* Flash Mode: QIO
* Flash Size: 1M (no SPIFFS)
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

# FAQ

<details>
<summary>1. Лог прошитой ESP без подключения к ватериусу</summary>
	
```
pio device monitor --port /dev/cu.wchusbserial1410 --baud 115200
--- Miniterm on /dev/cu.wchusbserial1410  115200,8,N,1 ---
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
;l␀d��|␀�$�|␃␄␌␄�␌$�␄c|ǃ␂�␛�{�c�␄c��o'�lno���␌b␜x�$c␇l{lx�n�␘␃␌␌�␌d␌��␌␄␌c␄g�|␃l�␌␄�c��'o�␀$��l`␃�␛␓no␄$`␃␏␃'{���o␄␄c␄�␏l␇s��'␄␌c␌�␇d�␃�␃$l�␒�$`␃��'�␂000:00:00:00:396  NOTICE    (ESP) : Booted
000:00:00:00:397  ERROR     (I2C) : end error:2
000:00:00:00:397  ERROR     (I2C) : get mode failed. Check i2c line.
000:00:00:00:400  NOTICE    (ESP) : Going to sleep
000:00:00:00:404  ERROR     (I2C) : end error:2
```

ЕSP включается, запрашивает режим включения у Attiny, нет ответа, идёт спать.
</details>

<details>
<summary>2. Проверка без ESP, что attiny прошилась</summary>
	
- Замыкаете на разъеме ESP выводы TX и EN.
- Жмёте кнопку 1 сек, отпускаете
- Проверяете, что загорелся светодиод — attiny прошита корректно
</details>
