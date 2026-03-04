# OTA обновление прошивки Waterius

OTA (Over-The-Air) позволяет обновлять прошивку и файловую систему Waterius удалённо, без физического подключения по USB.

**Поддерживаемые модели:** только Waterius-2 (MODEL_2, ESP-12F, flash 4MB).
ESP-01 (1MB flash) не поддерживает OTA — недостаточно места для staging area.

## Общая схема работы

```
ESP ──POST данные──> Сервер
ESP <──JSON ответ─── Сервер  (содержит "update_ota": "https://.../manifest.json")
ESP ──GET──────────> manifest.json
ESP ──GET──────────> filesystem.bin  (LittleFS, если есть в манифесте)
ESP ──GET──────────> firmware.bin
ESP ──перезагрузка──> Новая прошивка работает
```

Сервер решает, кому и когда отдавать OTA — ESP просто выполняет команду.

## Последовательность работы

### 1. Штатный цикл отправки данных

ESP просыпается (по таймеру или кнопке), подключается к WiFi и отправляет показания на сервер. В JSON-запросе всегда есть поле `version_esp` с текущей версией прошивки.

### 2. Сервер отвечает с командой OTA

Если сервер решает что устройству нужно обновление, он включает в ответ поле `update_ota` с URL манифеста:

```json
{"update_ota": "https://cloud.waterius.ru/ota/manifest.json"}
```

Если обновление не нужно — пустой ответ `{}`.

### 3. Проверки перед обновлением

Перед началом OTA выполняются проверки:

**Проверка батареи (двойная):**

| Проверка | Порог | Назначение |
|----------|-------|------------|
| `voltage.average()` > 4600 мВ | — | USB-питание, проверка пропускается |
| `voltage.average()` < 3300 мВ | Абсолютный порог | Защита NiMH (3 x 1.1В минимум) |
| `voltage.get_battery_level()` < 40% | Относительный порог | Защита Alkaline (по diff под нагрузкой) |

Двойная проверка нужна потому что:
- У NiMH аккумуляторов плоская кривая разряда → `battery%` завышен даже при почти разряженной батарее → ловим через абсолютный порог 3300 мВ
- У Alkaline батарей напряжение падает плавно → ловим через `battery%` < 40%
- При USB-питании `battery%` = 0% (diff большой из-за тока WiFi), но питание стабильно → пропускаем проверку если voltage > 4600 мВ

**Порог USB-питания — 4600 мВ** (не 4900). Под нагрузкой WiFi напряжение USB проседает до ~4830 мВ. Свежие 3xAA дают до ~4800 мВ, но при WiFi нагрузке падают до ~4700 мВ, что всё ещё > 4600.

**Валидация манифеста:**
- JSON должен быть валидным и не больше 1 КБ
- Должен содержать хотя бы одну секцию (`firmware` или `filesystem`)
- Каждая секция должна содержать `url`, `md5`, `size`
- URL должен начинаться с `https://`

**Проверка MD5:**
- `ESP8266httpUpdate` проверяет MD5 после скачивания, до записи во flash
- Если MD5 не совпадает — файл отбрасывается, старая прошивка не тронута

### 4. Обновление файловой системы

Если в манифесте есть секция `filesystem`:
- Вызывается `extendWakeUp()` — сброс таймера Attiny85 (120 секунд)
- `ESPhttpUpdate.updateFS()` скачивает и записывает LittleFS-образ
- Перезагрузка отключена (`rebootOnUpdate(false)`) — после FS нужно ещё обновить firmware

### 5. Обновление прошивки

Если в манифесте есть секция `firmware`:
- Вызывается `extendWakeUp()`
- `ESPhttpUpdate.update()` скачивает прошивку в staging area
- Перезагрузка отключена — после записи выполняется явная проверка результата

### 6. Перезагрузка

После успешного обновления всех компонентов:
```
OTA: filesystem updated OK
OTA: firmware updated OK
OTA: complete, restarting ESP...
```
ESP вызывает `ESP.restart()`. Bootloader (eboot) копирует прошивку из staging area в APP-область (~3.8 секунды для 637 КБ).

### 7. Проверочный цикл

После перезагрузки ESP проходит обычный цикл: WiFi → NTP → отправка данных. В JSON уже новая `version_esp`. Сервер видит новую версию и отвечает `{}` — OTA больше не запускается.

## Формат манифеста

```json
{
  "version": "2.0.25",
  "firmware": {
    "url": "https://cloud.waterius.ru/ota/nodemcuv2-2.0.25.bin",
    "size": 637200,
    "md5": "b66e9d1297bc1a170d140aeddc3e15b8"
  },
  "filesystem": {
    "url": "https://cloud.waterius.ru/ota/nodemcuv2-2.0.25-fs.bin",
    "size": 1024000,
    "md5": "cb310b8ebd91d701dc869b62fc048ab4"
  }
}
```

| Поле | Описание |
|------|----------|
| `version` | Информационное, для логирования |
| `firmware` | Прошивка ESP (опционально) |
| `filesystem` | LittleFS-образ с веб-файлами (опционально) |
| `url` | Полный HTTPS URL файла |
| `size` | Размер в байтах |
| `md5` | MD5-хеш файла |

Манифест должен содержать хотя бы одну из секций (`firmware` или `filesystem`).

## Коды ошибок OTA

Код ошибки сохраняется в `Settings.reserved9[0]` (EEPROM). При следующей отправке данных включается в JSON как `ota_error`. После успешной отправки обнуляется.

| Код | Константа | Описание |
|-----|-----------|----------|
| 0 | `OTA_ERR_NONE` | Нет ошибки |
| 1 | `OTA_ERR_DOWNLOAD` | Ошибка скачивания манифеста (нет связи, 404, и т.д.) |
| 2 | `OTA_ERR_PARSE` | Ошибка парсинга манифеста (невалидный JSON, нет полей) |
| 3 | `OTA_ERR_INVALID_URL` | URL не начинается с `https://` |
| 4 | `OTA_ERR_FS_UPDATE` | Ошибка обновления filesystem (MD5, обрыв, и т.д.) |
| 5 | `OTA_ERR_FW_UPDATE` | Ошибка обновления firmware (MD5, обрыв, и т.д.) |
| 6 | `OTA_ERR_TIMEOUT` | Таймаут скачивания |
| 7 | `OTA_ERR_LOW_BATTERY` | Батарея разряжена |

## Настройка сервера

### PHP-сниппет для WordPress (Code Snippets)

Минимальный сниппет, который управляет раздачей OTA:

```php
<?php
$OTA_ENABLED = true;
$OTA_TARGET_VERSION = '2.0.24'; // OTA только для этой версии ('' = для всех)
$OTA_MANIFEST_URL = 'https://cloud.waterius.ru/ota/manifest.json';

add_action('init', function() use ($OTA_ENABLED, $OTA_TARGET_VERSION, $OTA_MANIFEST_URL) {
    if ($_SERVER['REQUEST_URI'] !== '/cloud-waterius-ru') { return; }

    $input = file_get_contents('php://input');
    $data = json_decode($input, true);
    if ($data) { error_log('Waterius data: ' . print_r($data, true)); }

    $response = [];
    if ($OTA_ENABLED) {
        $version_esp = isset($data['version_esp']) ? $data['version_esp'] : '';
        if ($OTA_TARGET_VERSION === '' || $version_esp === $OTA_TARGET_VERSION) {
            $response['update_ota'] = $OTA_MANIFEST_URL;
        }
    }

    header('Content-Type: application/json');
    http_response_code(200);
    echo json_encode($response, JSON_FORCE_OBJECT);
    exit;
});
```

**Параметры:**

| Параметр | Описание |
|----------|----------|
| `$OTA_ENABLED` | `true` — отдавать OTA, `false` — обычный ответ `{}` |
| `$OTA_TARGET_VERSION` | Версия ESP для обновления. `'2.0.24'` — только 2.0.24 получит OTA. `''` — все версии |
| `$OTA_MANIFEST_URL` | URL манифеста на сервере |

**Защита от зацикливания:** сниппет проверяет `version_esp` из POST-данных. Если версия ESP уже обновлена — `update_ota` не отдаётся.

### Настройка release-сервера

1. Создать директорию для OTA-файлов на сервере (например `/ota/`)
2. Загрузить файлы:
   - `manifest.json`
   - `nodemcuv2-X.X.X.bin` (firmware)
   - `nodemcuv2-X.X.X-fs.bin` (filesystem)
3. Убедиться что файлы доступны по HTTPS
4. В PHP-сниппете указать `$OTA_MANIFEST_URL = 'https://cloud.waterius.ru/ota/manifest.json'`
5. Установить `$OTA_TARGET_VERSION` на версию, которую нужно обновить
6. Включить `$OTA_ENABLED = true`

### Отключение OTA после обновления

Два способа:
- **Автоматический:** `$OTA_TARGET_VERSION = '2.0.24'` — после обновления до 2.0.25 устройство перестанет получать OTA
- **Ручной:** `$OTA_ENABLED = false` — полностью отключить OTA для всех

## Сборка OTA-бинарей

### Скрипт сборки

```bash
cd ESP8266
./scripts/build_and_deploy.sh 2.0.25
```

Скрипт выполняет:
1. Временно подменяет `firmware_version` в `platformio.ini` на указанную версию
2. Собирает прошивку: `platformio run --environment waterius_2`
3. Собирает файловую систему: `platformio run --target buildfs --environment waterius_2`
4. Вычисляет MD5 и размеры
5. Копирует бинари в `ota/`
6. Генерирует `ota/manifest.json`
7. Восстанавливает `platformio.ini` (через trap, даже при ошибке)

Результат:
```
ESP8266/ota/
├── manifest.json
├── nodemcuv2-2.0.25.bin        # firmware (~637 KB)
└── nodemcuv2-2.0.25-fs.bin     # filesystem (~1000 KB)
```

### VSCode Task

В `.vscode/tasks.json` есть задача **OTA: Build and prepare**. При запуске запрашивает версию (по умолчанию 2.0.25).

### Переключение debug / release

В `scripts/build_and_deploy.sh` изменить `OTA_BASE_URL`:

```bash
# Debug
OTA_BASE_URL="https://us.pstd.ru/us-files/waterius"

# Release
OTA_BASE_URL="https://cloud.waterius.ru/ota"
```

### Загрузка на сервер

После сборки загрузить содержимое `ota/` на сервер. Все 3 файла должны быть доступны по HTTPS.

## Тайминги

Реальные замеры на Waterius-2, WiFi 802.11n, HTTPS, сервер в интернете:

### Полный цикл с OTA (FS + firmware)

```
00:00  ESP проснулся
00:04  WiFi подключён                         (~3.6с)
00:04  NTP синхронизация                       (~0.1с)
00:05  Отправка данных #1                      (~1с)
00:05  Сервер вернул update_ota
00:06  apply_settings + отправка данных #2     (~1с)
00:06  OTA: start
00:07  GET manifest.json                       (~0.6с, 350 байт)
00:07  OTA: downloading filesystem...
00:46  OTA: filesystem updated OK              (~39с, 1024 КБ)
00:46  OTA: downloading firmware...
01:13  OTA: firmware updated OK                (~27с, 637 КБ)
01:13  OTA: complete, restarting ESP...
       ──── перезагрузка (eboot ~3.8с) ────
03:54  Новая прошивка стартовала
08:00  Firmware ver: 2.0.25
09:06  Going to sleep
```

### Тайминги по этапам

| Этап | Время | Размер | Скорость |
|------|-------|--------|----------|
| WiFi подключение | ~3.6с | — | — |
| NTP синхронизация | ~0.1с | — | — |
| Отправка данных | ~1с | ~900 Б | — |
| Скачивание манифеста | ~0.6с | 350 Б | — |
| Обновление filesystem | ~39с | 1024 КБ | ~26 КБ/с |
| Обновление firmware | ~27с | 637 КБ | ~24 КБ/с |
| Перезагрузка (eboot copy) | ~3.8с | 637 КБ | — |
| **Полный цикл с OTA** | **~73с** | — | — |
| **Штатный цикл без OTA** | **~5-9с** | — | — |

Скорость скачивания ~24-26 КБ/с (HTTPS через WiFi 802.11n). Запас по таймеру Attiny: `extendWakeUp()` даёт 120 секунд, OTA занимает ~67 секунд — резерв ~53 секунды.

## Flash layout (ESP8266, 4MB)

```
┌──────────────────────────────────────┐
│ Bootloader (eboot): 4 KB            │ 0x000000
├──────────────────────────────────────┤
│ APP (текущая прошивка): ~637 KB      │ 0x001000
├──────────────────────────────────────┤
│ Staging area (свободно): ~2.4 MB     │ ← сюда скачивается новая прошивка
├──────────────────────────────────────┤
│ LittleFS: 1 MB                      │ 0x300000
├──────────────────────────────────────┤
│ EEPROM + WiFi config: 20 KB         │ 0x3FB000
└──────────────────────────────────────┘
```

ESP8266 **не имеет** dual-partition OTA (A/B) как ESP32. Используется staging area:

1. `ESP8266httpUpdate` скачивает новую прошивку в staging area
2. MD5 проверяется до записи команды bootloader
3. Если MD5 совпал — записывается команда для eboot в RTC-память
4. ESP перезагружается
5. eboot копирует прошивку из staging area в APP-область
6. Загружается новая прошивка

**Безопасность:**
- Если скачивание прервалось или MD5 не совпал — старая прошивка не тронута
- Единственный риск — сбой питания во время копирования eboot (шаг 5, ~1-2 сек). `extendWakeUp()` гарантирует что Attiny не отключит питание
- Обе копии (текущая + staging) помещаются между bootloader и LittleFS (~3 МБ, при прошивке ~637 КБ запас огромный)

**Обновление LittleFS:**
- `updateFS()` записывает образ напрямую в область LittleFS (0x300000)
- Не использует staging area — прямая запись

## Ограничения и риски

### Нет rollback

Если новая прошивка загрузилась но не работает — откат только через USB. Тестируйте прошивку перед публикацией на сервер.

### Только Waterius-2

OTA компилируется только для `WATERIUS_MODEL == WATERIUS_MODEL_2` (ESP-12F, 4MB flash). На ESP-01 (1MB) не хватает места для staging area.

### Расход батареи

OTA потребляет больше энергии: WiFi активен ~67 секунд вместо обычных ~5-9. Одно обновление не критично для 3xAA батарей, но не стоит запускать OTA часто.

### HTTPS без проверки сертификата

Используется `setInsecure()` (как и во всём проекте). Трафик шифруется, но сертификат сервера не проверяется. Для OTA это допустимо благодаря MD5-проверке содержимого.

### Обновление Attiny85 невозможно

ESP подключён к Attiny85 только по I2C — нет доступа к ISP-пинам. Обновление Attiny только через USB-программатор.

### Настройки после OTA

EEPROM (Settings) сохраняется при OTA — настройки WiFi, показания счётчиков и все пользовательские параметры не теряются. Настройки сбросятся только если в новой прошивке изменилась `CURRENT_VERSION` конфигурации.

## Структура файлов

```
ESP8266/
├── src/
│   ├── ota_update.h          # Константы и объявление perform_ota_update()
│   ├── ota_update.cpp         # Реализация OTA (скачивание, запись, рестарт)
│   ├── ota_manifest.h         # Парсинг и валидация манифеста (тестируемая логика)
│   ├── main.cpp               # Вызов OTA после получения update_ota от сервера
│   └── json.cpp               # Отправка ota_error в JSON
├── test/
│   └── test_ota/
│       ├── test_manifest.cpp  # Тесты парсинга манифеста (native, googletest)
│       └── test_url_validation.cpp  # Тесты валидации URL
├── scripts/
│   └── build_and_deploy.sh    # Скрипт сборки OTA-бинарей
├── ota/                       # Результат сборки (не в git)
│   ├── manifest.json
│   ├── nodemcuv2-X.X.X.bin
│   └── nodemcuv2-X.X.X-fs.bin
└── doc/
    └── ota.md                 # Этот документ
```

## Запуск тестов

```bash
cd ESP8266
platformio test --environment native
```

21 тест покрывает парсинг манифеста и валидацию URL (валидный манифест, только firmware, только FS, невалидный JSON, http:// URL, пустой URL, nullptr и др.).
