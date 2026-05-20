# Quickstart

Как можно прошить ATtiny & ESP:
1. Взять готовые hex/bin файлы и залить их
2. Скомпилировать исходный код в PlatformIO (CLI или Visual Studio Code)
3. Скомпилировать исходный код в Arduino IDE

---

# Подготовка среды для прошивки (Windows 11)

Для работы вам необходимо подготовить программное обеспечение и установить драйверы для используемых программаторов.

### Установка инструментов через терминал

Запустите **PowerShell** от имени администратора и выполните команды ниже.

#### Одной строкой (все сразу):
```powershell
winget install AVRDudes.AVRDUDE Akeo.Zadig astral-sh.uv Python.Python.3.14; uv tool install esptool
```

#### По отдельности с пояснениями:

*   **AVRDUDE** — утилита для работы с USBasp и контроллерами AVR:
    ```powershell
    winget install AVRDudes.AVRDUDE
    ```
*   **Zadig** — инсталлятор универсальных драйверов для USBasp:
    ```powershell
    winget install Akeo.Zadig
    ```
*   **uv** — быстрый менеджер пакетов для Python:
    ```powershell
    winget install astral-sh.uv
    ```
*   **Python 3.14** — необходимая среда исполнения:
    ```powershell
    winget install Python.Python.3.14
    ```
*   **esptool** — прошивальщик для чипов ESP32 / ESP8266:
    ```powershell
    uv tool install esptool
    ```

---

### 🔌 Установка драйверов (ручной этап)

#### 1. Драйвер для USBasp
После установки **Zadig** через терминал:
1. Подключите **USBasp** к компьютеру.
2. Запустите Zadig.
3. Выберите `Options` → `List All Devices`.
4. В списке выберите `USBasp`.
5. Установите тип драйвера: `libusb-win32`.
6. Нажмите **Replace Driver**.

#### 2. Драйвер для CH340 (USB-Serial)
Для работы с COM-портом скачайте и запустите официальный установщик:
*   🔗 **[Скачать драйвер CH340](https://www.wch-ic.com/download/file?id=270)**

---

### Проверка готовности
*   Для проверки **USBasp** введите: `avrdude -c usbasp -p m328p`
*   Для проверки **CH340** проверьте наличие порта в: `Диспетчер устройств → Порты (COM и LPT)`

---

# Оборудование и схемы подключения

## Чем прошить

#### ATtiny85:
- Программатором USBasp
- Платой Arduino, загрузив в неё скетч Arduino-as-ISP
- Другим программатором для AVR (например, USB-ISP)

#### ESP8266:
- USB-TTL 3.3V переходником
- Платой Arduino, подключившись к её RX, TX + делитель до 3.3V

---

## Фьюзы ATtiny85
**Значения:** `E:FF`, `H:DF`, `L:62`

---

## Распиновка для прошивки ATtiny через разъем ESP

Прошивка ATtiny осуществляется без выпайки её с платы через разъем для ESP (тип разъема: PBD-8).  
**Вид сверху:**

| **GND** | **SCK (15)** | **MOSI (16)** | NC  | 
| ---- | ---- | ---- | ---- |
| NC | **MISO (14)** | NC  | **VCC** |

+ 10-й пин на RESET ATtiny85

**Примечания:**
- **NC** — не используется
- **VCC** — подключить к любому 3.3V или 5V
- От программатора отдельными проводами необходимо подключиться в отверстия PBD-8
- ⚠️ **Не забыть:** отдельно подключить провод к пину RESET

---

## Программаторы для ATtiny

### Утилита Avrdude
- Официальный репозиторий: https://github.com/avrdudes/avrdude
- Альтернативная ссылка: http://www.avislab.com/blog/wp-content/uploads/2012/12/avrdude.zip

### Arduino в качестве ISP программатора (3.3V–5V)

1. Залейте скетч ISP программатора с помощью Arduino IDE в плату Arduino [[инструкция](http://www.martyncurrey.com/arduino-nano-as-an-isp-programmer/)]
2. Подключите плату Arduino к Вотериусу:

**Распиновка при прошивке с помощью Arduino Micro или Arduino UNO:**

| Micro | UNO/NANO | ISP | ATtiny85 |   
| ---- | ---- | ---- | ---- |
| 15 pin | 13 pin | SCK | 7 pin |
| 14 pin | 12 pin | MISO | 6 pin |
| 16 pin | 11 pin | MOSI | 5 pin |
| 10 pin | 10 pin | RESET | 1 pin |

+ Питание!

**Настройки в `platformio.ini`:**
```ini
upload_protocol = arduino
upload_flags = -P$UPLOAD_PORT
upload_speed = 19200
```

### Китайский USB-ISP программатор
- Плата: MX-USBISP-V5.00
- Программа: [ProgISP V1.7.2](https://yandex.ru/search/?text=ProgISP%20V1.7.2&&lr=213)
- Фьюзы: `E:FF`, `H:DF`, `L:62`

### USBasp программатор
Я купил китайский USB-ISP и перепрошил его по [инструкции](https://vochupin.blogspot.com/2016/12/usb-isp.html) в USBasp ([прошивка](https://www.fischl.de/usbasp/)). В диспетчере устройств он стал виден как USBasp.

- Драйвер: [v3.0.7](http://www.myrobot.ru/downloads/programs/usbasp-win-driver-x86-x64-v3.0.7.zip)

**Настройки в `platformio.ini`:**
```ini
upload_protocol = usbasp
upload_flags = 
    -Pusb 
    -B5
```

**Примечание:** в Windows 7 почему-то не заработал. Windows 10 x64 — OK.

---

## Программаторы для ESP8266

Для прошивки ESP8266 необходим USB-TTL преобразователь с логическим уровнем 3.3V. Обратите внимание, что у него должен быть регулятор напряжения для питания ESP8266 на 3.3V. У обычных USB-TTL преобразователей логический уровень 5V, поэтому их вывод TX нужно подключить к делителю напряжения. Я использую резисторы 1.5к и 2.2к.

[Инструкция из интернета](http://cordobo.com/2300-flash-esp8266-01-with-arduino-uno)  
(в большинстве других туториалах подключают 5V логику и делают ESP больно)

#### Драйверы для USB-TTL:
- [CH340G](https://all-arduino.ru/drajver-ch340g-dlya-arduino/)
- [PL2303](http://www.prolific.com.tw/US/ShowProduct.aspx?p_id=225&pcid=41)

---

# Прошивка готовыми бинарными файлами

Готовые прошивки доступны [на странице releases](https://github.com/dontsovcmc/waterius/releases).

## Прошивка ATtiny85 с помощью Avrdude & USBasp

1. **Установка Avrdude:**
   - Скачать: http://download.savannah.gnu.org/releases/avrdude/avrdude-6.2-mingw32.zip
   - **Для Windows 11:** открыть командную строку или PowerShell и выполнить:
     ```powershell
     winget install AVRDudes.AVRDUDE
     ```

2. Распакуйте архив, зайдите в папку. Откройте консоль: `Shift` + правая кнопка мыши → **Открыть окно команд**.

3. Скачайте прошивку ATtiny85:
   ```bash
   curl https://raw.githubusercontent.com/dontsovcmc/waterius_firmware/master/0.5/attiny85.hex --output ./attiny85.hex
   ```
   Если нет `curl`, то откройте ссылку в браузере и скопируйте файл в папку Avrdude.

4. Установите драйвер программатора [USBasp](http://www.myrobot.ru/downloads/driver-usbasp-v-2.0-usb-isp-windows-7-8-10-xp.php) и подключите его к ATtiny85.

5. Установите фьюзы:
   ```bash
   avrdude.exe -p t85 -c Usbasp -B 4 -P usb -U efuse:w:0xFF:m -U hfuse:w:0xDF:m -U lfuse:w:0x62:m
   ```

6. Прошейте микроконтроллер:
   ```bash
   avrdude.exe -p t85 -c Usbasp -B 4 -P usb -U flash:w:"D:\firmware.hex":a
   ```
   где `D:\firmware.hex` — путь до вашего файла.

---

## Прошивка ESP8266

Программатор не нужен, а нужен переходник с USB на TTL 3.3V.

1. Установите Python 3.8 (2.7 тоже подойдёт, только пути для PATH другие).
   
   **Windows:** добавьте в PATH:
   ```
   C:\Users\Админ\AppData\Local\Programs\Python\Python38-32
   ```

2. Скачайте pip. Выполните `python get-pip.py` — Python установит утилиту pip.
   
   Добавьте в PATH:
   ```
   C:\Users\Админ\AppData\Local\Programs\Python\Python38-32\Scripts
   ```

3. Установите esptool:
   ```bash
   pip install esptool
   ```

4. Скачайте [прошивку ESP8266](https://github.com/dontsovcmc/waterius/releases) (файл `esp8266.bin`).

5. Подключите USB-TTL к ESP8266, замкнув **GPIO0** на **GND**.

6. Выполните прошивку:
   ```bash
   python -m esptool --port COM7 --baud 115200 write_flash --flash_freq 40m --flash_size 1MB --flash_mode qio 0x0 esp8266-1.0.2.bin 0xbb000 esp8266-1.0.2-fs.bin
   ```
   
   **COM7** замените на свой порт.

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
```bash
python -m esptool --chip esp8266 --port COM3 --after no_reset erase_flash
```

---

# Прошивка через PlatformIO

### Установка PlatformIO

PlatformIO бывает в виде консольной утилиты или как дополнение в Visual Studio Code.

- [Инструкция по установке утилиты](http://docs.platformio.org/en/latest/installation.html#python-package-manager)
- [Инструкция из интернета](https://medium.com/jungletronics/attiny85-easy-flashing-through-arduino-b5f896c48189)

У нас ATtiny85 уже сидит на плате, поэтому подключаемся к разъему.

После установки в командной строке можно вызывать `platformio --version` и увидеть версию PlatformIO.

### Прошивка ATtiny

1. Откройте в командной строке папку `waterius/Attiny85`
2. Измените в файле `platformio.ini` порт на свой:
   ```ini
   upload_port = /dev/tty.usbmodem1421
   ```
3. Выполните:
   ```bash
   platformio run --target upload
   ```

### Прошивка ESP8266

1. Откройте в командной строке папку `waterius/ESP8266`
2. Измените в файле `platformio.ini` порт на свой:
   ```ini
   upload_port = /dev/tty.usbmodem1411
   ```
3. Прошейте сначала файл прошивки:
   ```bash
   platformio run --target upload --environment esp01_1m
   ```
4. Затем прошейте файловую систему:
   ```bash
   platformio run --target uploadfs --environment esp01_1m
   ```

---

# Прошивка с помощью Arduino IDE

## ATtiny

1. Установить [поддержку ATtiny плат](https://github.com/SpenceKonde/ATTinyCore/blob/master/Installation.md)
2. Выбрать **1 MHz internal**
3. В `/attiny/src` переименовать `main.cpp` в `src.ino`
4. Открыть `src.ino` в Arduino IDE
5. Компилировать

## ESP8266

### Требуемые библиотеки

* Blynk by Volodymyr Shymanskyy (0.6.1)
* ArduinoJSON (6.12.0)
* PubSubClient (2.7.0)
* Установить вручную из zip [WiFiManager#waterius_release_101](https://github.com/dontsovcmc/WiFiManager/tree/waterius_release_101)

**Примечание:** актуальные версии в [platformio.ini](https://github.com/dontsovcmc/waterius/blob/master/ESP8266/platformio.ini)

### Настройка Arduino IDE для ESP8266

**Additional Boards Managers URLs:**
```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

**Board settings:**
* **Board:** Generic ESP8266 Module
* **Flash Mode:** QIO
* **Flash Size:** 1M (no SPIFFS)
* **Debug port:** Disable
* **Debug Level:** None
* **lwIP Variant:** v2 Lower Memory
* **Reset Method:** ck
* **Crystal Frequency:** 26 MHz
* **Flash Frequency:** 40MHz
* **CPU Frequency:** 80 MHz
* **Built-in Led:** 0
* **Upload Speed:** 115200
* **Port:** select your port

---

# FAQ

<details>
<summary>1. Лог прошитой ESP без подключения к ватериусу</summary>

```
pio device monitor --port /dev/cu.wchusbserial1410 --baud 115200
--- Miniterm on /dev/cu.wchusbserial1410  115200,8,N,1 ---
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
;l␀d|␀$|␃␄␌␄␌$␄c|ǃ␂␛{c␄co'lno␌b␜x$c␇l{lxn␘␃␌␌␌d␌␌␄␌c␄g|␃l␌␄c'o␀$l`␃␛␓no␄$`␃␏␃'{o␄␄c␄␏l␇s'␄␌c␌␇d␃␃$l␒$`␃'␂000:00:00:00:396  NOTICE    (ESP) : Booted
000:00:00:00:397  ERROR     (I2C) : end error:2
000:00:00:00:397  ERROR     (I2C) : get mode failed. Check i2c line.
000:00:00:00:400  NOTICE    (ESP) : Going to sleep
000:00:00:00:404  ERROR     (I2C) : end error:2
```

ЕSP включается, запрашивает режим включения у ATtiny, нет ответа, идёт спать.
</details>

<details>
<summary>2. Проверка без ESP, что ATtiny прошилась</summary>

- Замыкаете на разъеме ESP выводы TX и EN.
- Жмёте кнопку 1 сек, отпускаете
- Проверяете, что загорелся светодиод — ATtiny прошита корректно
</details>
