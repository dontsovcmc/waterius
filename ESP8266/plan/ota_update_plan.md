# План: OTA обновление прошивки Waterius ESP-12F

## Общая концепция

Сервер (или эмулятор на WordPress) в ответе на отправку данных может включить поле `update_ota` со ссылкой на JSON-манифест. ESP скачивает манифест, затем обновляет прошивку и/или файловую систему LittleFS.

```
ESP -> POST данные -> Сервер
ESP <- JSON ответ {... "update_ota": "https://us.pstd.ru/us-files/waterius/manifest.json" ...}
ESP -> GET manifest.json -> Проверяет что обновлять
ESP -> GET firmware.bin  -> Прошивает через ESP8266httpUpdate
ESP -> GET littlefs.bin  -> Обновляет файловую систему
ESP -> Перезагрузка (автоматическая после OTA)
```

---

## 1. Серверные пути (debug / release)

URL задаются через константы сборки в `platformio.ini`. ESP не знает про конкретные пути — получает URL манифеста из ответа сервера. Но PHP сниппет и скрипт сборки используют эти пути.

| | Debug (разработка) | Release (продакшн) |
|---|---|---|
| **Endpoint данных** | `https://us.pstd.ru/cloud-waterius-ru` | `https://cloud.waterius.ru` |
| **OTA бинари** | `https://us.pstd.ru/us-files/waterius/` | `https://cloud.waterius.ru/ota/` |
| **Манифест** | `https://us.pstd.ru/us-files/waterius/manifest.json` | `https://cloud.waterius.ru/ota/manifest.json` |

Константы `platformio.ini` (build_flags):
```
-DOTA_BASE_URL=\"https://us.pstd.ru/us-files/waterius/\"    ; debug
-DOTA_BASE_URL=\"https://cloud.waterius.ru/ota/\"            ; release
```

PHP сниппет и скрипт сборки используют `OTA_BASE_URL` для формирования URL в манифесте.

---

## 2. Формат манифеста (manifest.json)

Файл на сервере `https://us.pstd.ru/us-files/waterius/manifest.json`:

```json
{
  "version": "2.0.25",
  "firmware": {
    "url": "https://us.pstd.ru/us-files/waterius/nodemcuv2-2.0.25.bin",
    "size": 615432,
    "md5": "d41d8cd98f00b204e9800998ecf8427e"
  },
  "filesystem": {
    "url": "https://us.pstd.ru/us-files/waterius/nodemcuv2-2.0.25-fs.bin",
    "size": 1048576,
    "md5": "098f6bcd4621d373cade4e832627b4f6"
  }
}
```

- `firmware` — обязательный блок, прошивка ESP
- `filesystem` — опциональный, если нужно обновить веб-файлы LittleFS
- `md5` — проверка целостности (ESP8266httpUpdate поддерживает MD5 нативно через HTTP-заголовок `x-MD5`)
- `version` — информационное поле для логирования

**Почему манифест, а не прямая ссылка на бинарь:**
- Позволяет обновлять firmware и filesystem независимо
- Можно добавить только `firmware` без `filesystem` и наоборот
- MD5 проверяется до начала записи во flash
- Один URL в ответе сервера вместо нескольких полей

---

## 3. Принципы реализации

### 2.0 Ключевые требования к коду

1. **Экономия heap** — на ESP8266 мало оперативной памяти (~40-50KB свободно). Все строки через `F()` макрос (хранение во flash). Не выделять большие буферы. `JsonDocument` для манифеста создавать в локальном скоупе — освобождается при выходе. Переиспользовать WiFiClient. Не копировать URL в String без необходимости — передавать `const char*`.

2. **Простой код** — одна функция `perform_ota_update()`, линейная логика без абстракций. Минимум ветвлений. Код должен быть читаем без комментариев.

3. **Безопасность** — MD5-проверка перед записью во flash (встроена в ESP8266httpUpdate). Валидация URL из манифеста (проверка что начинается с `https://`). Ограничение размера манифеста (не более 1KB). Таймаут на скачивание. Не доверять данным от сервера без проверки.

4. **Проверка всех ошибок** — каждый вызов (HTTP-запрос, парсинг JSON, OTA-запись) проверяется на ошибку. При любой ошибке — сохранить код в `reserved9[0]`, залогировать и вернуть false. Не продолжать обновление если предыдущий шаг упал (FS не записался → firmware не трогаем).

5. **Проверка батареи перед OTA** — OTA запрещена при низком заряде. Во время скачивания WiFi активен ~30 сек (вместо обычных ~10), ток ~70-170мА. При слабой батарее просадка напряжения может вызвать brownout и прервать запись во flash.
   - Проверка: `voltage.get_battery_level() < OTA_MIN_BATTERY_PERCENT` → отказ от OTA, код ошибки 7
   - `OTA_MIN_BATTERY_PERCENT = 40` — запас ~15-20% сверх порога low_voltage (который при 0% = просадка >=100мВ или среднее <2900мВ)
   - Расчёт порога: при battery_level 25% diff уже >50мВ (батарея начала разряжаться). Для OTA нужен запас на ~20 сек дополнительного WiFi TX. При 40% diff ещё мал (~17мВ), среднее напряжение ~3.5В+ — достаточно для OTA даже с просадкой 300-400мВ
   - Проверка делается **до** скачивания манифеста — экономит батарею при отказе

6. **Экономия батареи** — WiFi уже подключён после `send_data()`, не создавать новое подключение. Не включать WiFi специально для OTA. Скачивание начинается сразу без задержек. После OTA (успех или ошибка) — сразу к завершению цикла.

7. **Максимальная скорость обновления** — не проверять версию на ESP (сервер решает). Не скачивать манифест если `update_ota` пуст. Один HTTP-клиент на весь процесс. Сначала FS (без перезагрузки), потом firmware (с перезагрузкой) — за один цикл бодрствования.

8. **Сохранение ошибок OTA** — код ошибки (1 байт) в `Settings.reserved9[0]`. При следующей отправке данных код включается в JSON как `ota_error`. После отправки обнуляется. Сервер узнаёт о проблеме и может решить повторить или отменить OTA.

---

## 4. Изменения в ESP8266 прошивке

### 3.1 Хранение ошибки OTA в Settings

В `setup.h` — один байт из `reserved9[74]`:

```
reserved9[0] — ota_error (uint8_t): 0=нет ошибки, 1-6=код ошибки
reserved9[1..73] — свободно
```

| Код | Значение |
|-----|----------|
| 0   | Нет ошибки |
| 1   | Ошибка скачивания манифеста |
| 2   | Ошибка парсинга манифеста |
| 3   | Невалидный URL в манифесте |
| 4   | Ошибка обновления filesystem |
| 5   | Ошибка обновления firmware |
| 6   | Таймаут скачивания |
| 7   | Батарея разряжена (< 40%) |

### 3.2 Новый файл: `src/ota_update.h` / `src/ota_update.cpp`

```cpp
// ota_update.h
#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include "master_i2c.h"
#include "setup.h"

// Выполнить OTA обновление по URL манифеста.
// WiFi должен быть уже подключён.
// При успехе ESP перезагрузится автоматически (функция не вернёт управление).
// При ошибке — сохраняет код ошибки в sett.reserved9[0] и возвращает false.
bool perform_ota_update(const char *manifest_url, MasterI2C &masterI2C, Settings &sett);

#endif
```

**Алгоритм `perform_ota_update()`:**

```
1. Проверить voltage.get_battery_level() >= OTA_MIN_BATTERY_PERCENT (40%)
   Если нет → reserved9[0] = 7, return false
2. extendWakeUp() — гарантировать время на скачивание
3. HTTP GET manifest_url → читаем тело (лимит 1KB)
4. Парсим JSON (JsonDocument на стеке, маленький — манифест ~200 байт)
5. Извлекаем URL и MD5 для firmware и filesystem
6. Валидация URL (начинается с https://)
7. Если есть "filesystem":
   a. extendWakeUp()
   b. ESPhttpUpdate.rebootOnUpdate(false)
   c. ESPhttpUpdate.updateFS(client, fs_url)
   d. Ошибка → reserved9[0] = код, return false
8. Если есть "firmware":
   a. extendWakeUp()
   b. ESPhttpUpdate.rebootOnUpdate(true)
   c. ESPhttpUpdate.update(client, fw_url)
   d. Ошибка → reserved9[0] = код, return false
   e. Успех → ESP перезагрузится, сюда не дойдёт
9. Если обновлялся только FS → ESP.restart()
```

**Экономия heap в реализации:**
- `WiFiClientSecure` создаётся один раз, переиспользуется для манифеста и бинарей
- Манифест парсится из HTTP stream напрямую (`deserializeJson(doc, httpClient.getStream())`) — без промежуточной String
- URL из манифеста читаются через `const char*` из JsonDocument (zero-copy в ArduinoJson 7)
- После парсинга манифеста JsonDocument выходит из скоупа — память освобождается до начала скачивания бинаря

### 3.3 Изменения в `main.cpp`

Вставить обработку OTA **после** `apply_settings()` и **до** `wifi_shutdown()`:

```
Текущий код (строки 127-136):
  send_data(sett, data, cdata, json_data, json_settings_received);
  if (settings_received(json_settings_received)) {
      apply_settings(json_settings_received);
      send_data(sett, data, cdata, json_data, json_settings_received);
  }
+ // --- OTA UPDATE ---
+ if (json_settings_received[F("update_ota")].is<const char*>()) {
+     perform_ota_update(json_settings_received[F("update_ota")], masterI2C, sett);
+ }
  wifi_shutdown();
```

**Важно:** Если OTA успешно — `ESPhttpUpdate` сам перезагрузит ESP. Код после `perform_ota_update()` выполняется только при ошибке.

**Почему `is<const char*>()` а не `containsKey()`:** проверяет и наличие ключа, и что значение — строка. Безопаснее.

### 3.4 Отправка ошибки OTA в JSON

В `json.cpp` при формировании данных — если `sett.reserved9[0] != 0`, добавить:
```cpp
if (sett.reserved9[0] != 0) {
    root[F("ota_error")] = (int)sett.reserved9[0];
}
```
После успешной отправки — обнулить `sett.reserved9[0]`.

### 3.5 Обработка поля `update_ota` в `apply_settings()`

Поле `update_ota` **не** обрабатывается в `apply_settings()` — та просто не найдёт совпадения для неизвестного ключа и пропустит его. Дополнительных изменений не нужно.

### 3.6 Управление питанием во время OTA

```
Временная шкала OTA:
├─ 0с: ESP проснулся, отправил данные (~5-10с)
├─ 10с: Получен ответ с update_ota
├─ 10с: extendWakeUp() → таймер Attiny сброшен (ещё 120с)
├─ 11с: GET manifest.json (~1с)
├─ 12с: extendWakeUp() перед скачиванием
├─ 12-25с: Скачивание и запись (~600KB, 5-15с)
├─ ~25с: Автоматическая перезагрузка
└─ Attiny видит перезагрузку, продолжает штатный цикл
```

WiFi уже подключён после `send_data()` — не нужно подключаться заново.
Никаких `delay()` или ожиданий — сразу к делу.
**120 секунд** от `extendWakeUp()` с запасом хватит на весь процесс.

---

## 5. PHP сниппет для WordPress (Code Snippets)

Сниппет эмулирует ответ сервера waterius.ru. Устанавливается через плагин Code Snippets.

В настройках Waterius указать URL: `https://us.pstd.ru/cloud-waterius-ru` (или другой endpoint).

```php
<?php
/**
 * Эмулятор сервера Waterius с поддержкой OTA
 * Плагин: Code Snippets (WordPress)
 *
 * Настройки (менять здесь):
 */
$OTA_ENABLED = true;  // true = отдавать update_ota, false = обычный ответ
$OTA_MANIFEST_URL = 'https://us.pstd.ru/us-files/waterius/manifest.json'; // debug
// $OTA_MANIFEST_URL = 'https://cloud.waterius.ru/ota/manifest.json';    // release

add_action('init', function() use ($OTA_ENABLED, $OTA_MANIFEST_URL) {
    // URL: https://us.pstd.ru/cloud-waterius-ru
    if ($_SERVER['REQUEST_URI'] !== '/cloud-waterius-ru') {
        return;
    }

    // Читаем тело POST запроса
    $input = file_get_contents('php://input');
    $data = json_decode($input, true);

    // Логируем входящие данные (опционально)
    if ($data) {
        error_log('Waterius data: ' . print_r($data, true));
    }

    // Формируем ответ
    $response = [];

    if ($OTA_ENABLED) {
        $response['update_ota'] = $OTA_MANIFEST_URL;
    }

    header('Content-Type: application/json');
    http_response_code(200);
    echo json_encode($response);
    exit;
});
```

**Использование:**
1. Установить плагин Code Snippets на WordPress
2. Создать новый PHP-сниппет с кодом выше
3. В настройках Waterius (captive portal) указать HTTP URL: `https://us.pstd.ru/cloud-waterius-ru`
4. Для включения/выключения OTA менять `$OTA_ENABLED = true/false`

---

## 6. Сборка прошивки и подготовка OTA файлов

Скрипт `plan/build_and_deploy.sh` автоматизирует весь процесс:

```bash
cd ESP8266
./plan/build_and_deploy.sh
```

Скрипт выполняет:
1. Извлекает версию из `platformio.ini` (`firmware_version`)
2. Собирает прошивку: `platformio run --environment waterius_2`
3. Собирает файловую систему: `platformio run --target buildfs --environment waterius_2`
4. Вычисляет MD5 и размеры файлов
5. Создаёт папку `ota/` и копирует туда бинари с версией в имени
6. Генерирует `ota/manifest.json` с правильными URL, MD5 и размерами

### Результат в `ota/`

```
ESP8266/ota/
├── manifest.json
├── nodemcuv2-2.0.25.bin
└── nodemcuv2-2.0.25-fs.bin
```

Содержимое `ota/` нужно загрузить на сервер в соответствующую директорию (debug или release).

### Переключение debug/release

В `plan/build_and_deploy.sh` раскомментировать нужную строку `OTA_BASE_URL`:
```bash
OTA_BASE_URL="https://us.pstd.ru/us-files/waterius"    # debug
# OTA_BASE_URL="https://cloud.waterius.ru/ota"          # release
```

---

## 7. Порядок обновления: firmware vs filesystem

**Важный нюанс:** если обновляются оба — сначала filesystem, потом firmware.

Причина: после записи firmware ESP автоматически перезагрузится. Если filesystem ещё не обновлён — новая прошивка может быть несовместима со старыми веб-файлами.

```
Порядок в perform_ota_update():
1. Скачать манифест
2. Если есть filesystem → обновить FS (ESPhttpUpdate.updateFS)
   - ESPhttpUpdate.rebootOnUpdate(false) — отключить авто-перезагрузку
3. Если есть firmware → обновить прошивку (ESPhttpUpdate.update)
   - ESPhttpUpdate.rebootOnUpdate(true) — перезагрузка после записи
4. Если обновлялся только FS → ESP.restart() вручную
```

---

## 8. Обработка ошибок

### 7.1 Таблица ошибок и действий

| Ситуация | Действие | Код |
|----------|----------|-----|
| Батарея < 40% | Код 7, штатный сон (не начинаем OTA) | 7 |
| Манифест не скачался | Сохранить код 1, штатный сон | 1 |
| JSON манифеста невалиден | Сохранить код 2, штатный сон | 2 |
| URL не начинается с https:// | Сохранить код 3, штатный сон | 3 |
| Ошибка обновления FS | Код 4, штатный сон (firmware не трогаем) | 4 |
| Ошибка обновления firmware | Код 5, штатный сон | 5 |
| Таймаут (нет ответа сервера) | Код 6, штатный сон | 6 |
| MD5 не совпал | ESPhttpUpdate отклонит запись | 4/5 |
| Обрыв скачивания | ESPhttpUpdate вернёт ошибку, flash не повреждён | 4/5 |
| WiFi пропал | ESPhttpUpdate вернёт ошибку | 4/5 |
| Attiny отключил питание | flash может повредиться — `extendWakeUp()` критичен | — |

### 7.2 Сохранение и отправка ошибки

```
При ошибке OTA:
1. sett.reserved9[0] = код_ошибки (1 байт)
2. store_config(sett) — записать в EEPROM чтобы не потерять при перезагрузке
3. return false → main.cpp продолжит к wifi_shutdown() и сну

При следующем пробуждении:
1. json.cpp: если reserved9[0] != 0, добавить ota_error (число) в JSON
2. После успешного send_data(): обнулить reserved9[0], store_config()
3. Сервер получит код ошибки и может решить повторить или отменить OTA
```

### 7.3 Защита от бесконечного цикла обновления

Если сервер всегда отвечает `update_ota` — ESP будет пытаться обновляться каждый цикл. Это расходует батарею, но не опасно (при ошибке — штатный сон).

Защита на стороне PHP сниппета:
- Вручную отключить `$OTA_ENABLED` после успешного обновления
- Логировать `version_esp` из запроса — если версия уже новая, не отдавать `update_ota`

Защита на стороне ESP (опционально, на будущее):
- Сравнивать `version` из манифеста с `FIRMWARE_VERSION`
- Если совпадают — пропустить обновление

---

## 9. Как работает OTA на ESP8266 (staging area)

ESP8266 **не имеет dual-partition OTA** как ESP32 (нет A/B переключения). Используется **staging area** механизм:

```
Flash layout waterius_2 (eagle.flash.4m1m.ld, 4MB):
┌──────────────────────────────────────┐
│ Bootloader (eboot): 4 KB            │ 0x000000
├──────────────────────────────────────┤
│ APP (текущая прошивка): ~612 KB      │ 0x001000
├──────────────────────────────────────┤
│ Staging area (свободно): ~2.4 MB     │ ← сюда скачивается новая прошивка
├──────────────────────────────────────┤
│ LittleFS: 1 MB                      │ 0x300000
├──────────────────────────────────────┤
│ EEPROM + WiFi: 20 KB                │ 0x3FB000
└──────────────────────────────────────┘
```

**Процесс OTA:**
1. `ESP8266httpUpdate` скачивает новую прошивку в **staging area** (между APP и LittleFS)
2. MD5 проверяется **до** записи команды для bootloader
3. Если MD5 совпал — записывается команда для `eboot` в RTC-память
4. ESP перезагружается
5. `eboot` копирует прошивку из staging area → APP (перезаписывает старую)
6. Загружается новая прошивка

**Безопасность механизма:**
- Если скачивание прервалось или MD5 не совпал — старая прошивка **не тронута**, ESP работает как раньше
- Единственный риск — сбой питания **во время копирования** eboot (шаг 5). Это очень короткое окно (~1-2 сек для 600KB). `extendWakeUp()` гарантирует что Attiny не отключит питание в этот момент
- **Rollback невозможен** — если новая прошивка загрузилась но не работает, откат только через USB

**Ограничение размера:** обе копии (текущая + staging) должны поместиться между bootloader и LittleFS (~3MB). При текущей прошивке ~612KB — запас огромный (~2.4MB свободно).

---

## 10. Ограничения и риски

1. **Нет rollback** — если новая прошивка загрузилась но не работает, откат только через USB. Поэтому важно тестировать прошивку перед публикацией на сервер.

2. **На ESP-01 (1MB flash) OTA невозможна** — при текущем размере прошивки ~612KB не хватит места для staging area. OTA только для waterius_2.

3. **Только для waterius_2 (MODEL_2, ESP-12F)** — `#if WATERIUS_MODEL == WATERIUS_MODEL_2` условная компиляция OTA.

4. **Расход батареи** — OTA потребляет больше энергии (WiFi активен ~30с вместо ~10с). Одно обновление не критично для 3xAA батарей.

5. **HTTPS** — `ESPhttpUpdate` поддерживает HTTPS с `setInsecure()` (как уже используется в проекте).

6. **Обновление Attiny85 невозможно** — ESP подключён к Attiny только по I2C, нет доступа к ISP-пинам. Обновление Attiny только через USB-программатор.

---

## 11. Файлы для создания/изменения

| Файл | Действие |
|------|----------|
| `src/ota_update.h` | Создать — объявление `perform_ota_update()` |
| `src/ota_update.cpp` | Создать — реализация OTA через манифест (~80-100 строк) |
| `src/ota_manifest.h` | Создать — парсинг и валидация манифеста (чистая логика, тестируемая) |
| `src/main.cpp` | Изменить — добавить 3 строки вызова OTA |
| `src/json.cpp` | Изменить — добавить `ota_error` в JSON если есть |
| `src/setup.h` | Без изменений кода (используем существующий `reserved9`) |
| `platformio.ini` | Изменить — добавить `[env:native]` для googletest |
| `test/test_ota/test_manifest.cpp` | Создать — native-тесты парсинга манифеста |
| `test/test_ota/test_url_validation.cpp` | Создать — native-тесты валидации URL |
| `plan/manifest.json.example` | Создать — пример манифеста |
| `plan/waterius_ota_snippet.php` | Создать — PHP сниппет для WordPress |
| `plan/build_and_deploy.sh` | Создать — скрипт сборки и генерации манифеста |

---

## 12. Native тесты (googletest)

В проекте пока нет native-тестов. Добавляем `[env:native]` в platformio.ini и тесты в `test/`.

### 11.1 Что тестируем native (без железа)

Выделяем чистую логику из `ota_update.cpp` в отдельный header `ota_manifest.h` — парсинг и валидация манифеста. Эти функции не зависят от ESP8266 (только ArduinoJson) и тестируются на хосте.

**Тестируемые функции:**

```cpp
// ota_manifest.h — чистая логика, тестируемая native

struct OtaManifest {
    const char *fw_url;
    const char *fw_md5;
    size_t fw_size;
    const char *fs_url;
    const char *fs_md5;
    size_t fs_size;
    bool has_firmware;
    bool has_filesystem;
};

// Парсит JSON манифест. Возвращает 0 при успехе, код ошибки при ошибке.
uint8_t parse_manifest(const char *json, size_t len, OtaManifest &manifest);

// Проверяет что URL начинается с https://
bool is_valid_ota_url(const char *url);
```

### 11.2 Тест-кейсы

**test_manifest.cpp:**
```
- Валидный манифест с firmware и filesystem → успех, все поля заполнены
- Манифест только с firmware → has_firmware=true, has_filesystem=false
- Манифест только с filesystem → has_firmware=false, has_filesystem=true
- Пустой JSON {} → ошибка 2
- Невалидный JSON "{{" → ошибка 2
- Манифест без url в firmware → ошибка 2
- Манифест без md5 в firmware → ошибка 2
- Манифест с http:// URL → ошибка 3
- Манифест с пустым URL → ошибка 3
- Очень длинный JSON (>1KB) → ошибка 2 (переполнение JsonDocument)
```

**test_url_validation.cpp:**
```
- "https://us.pstd.ru/file.bin" → true
- "http://us.pstd.ru/file.bin" → false
- "" → false
- nullptr → false
- "ftp://server/file" → false
- "https://" → false (слишком короткий)
```

### 11.3 Настройка PlatformIO native

Добавить в `platformio.ini`:

```ini
[env:native]
platform = native
test_framework = googletest
build_flags = -std=c++17
lib_deps = ArduinoJson@7.3.1
test_filter = test_ota/*
```

**Запуск тестов:**
```bash
cd ESP8266
platformio test --environment native
```

### 11.4 Структура файлов тестов

```
ESP8266/
├── test/
│   └── test_ota/
│       ├── test_manifest.cpp      — тесты парсинга манифеста
│       └── test_url_validation.cpp — тесты валидации URL
├── src/
│   ├── ota_manifest.h             — чистая логика (тестируемая)
│   ├── ota_update.h               — интерфейс OTA (зависит от ESP)
│   └── ota_update.cpp             — реализация (WiFi, HTTP, I2C)
```

---

## 13. Этапы реализации

Реализация пошаговая — каждый этап заканчивается проверкой/тестом. Следующий этап начинается только после успешной проверки предыдущего.

---

### Этап 1. Native тесты + тестируемая логика

**Задачи:**
- [x] Добавить `[env:native]` в `platformio.ini` (googletest + ArduinoJson)
- [x] Создать `src/ota_manifest.h` — `parse_manifest()` и `is_valid_ota_url()` (чистая логика)
- [x] Создать `test/test_ota/test_manifest.cpp` — тесты парсинга манифеста
- [x] Создать `test/test_ota/test_url_validation.cpp` — тесты валидации URL

**Проверка:**
```bash
platformio test --environment native
```
- [x] Все тесты проходят (21 тест)
- [x] Покрыты: валидный манифест, только firmware, только FS, пустой JSON, невалидный JSON, http:// URL, пустой URL, nullptr

---

### Этап 2. Подготовка серверной части

**Задачи:**
- [x] Создать `plan/manifest.json.example` — пример манифеста
- [x] Создать `plan/waterius_ota_snippet.php` — PHP сниппет для WordPress
- [x] Создать `plan/build_and_deploy.sh` — скрипт сборки и генерации манифеста

**Проверка:**
- [x] Установить сниппет на WordPress, вызвать `curl -X POST https://us.pstd.ru/cloud-waterius-ru` — получить JSON с `update_ota`
- [x] Вызвать с `$OTA_ENABLED = false` — получить пустой JSON `{}`
- [x] Положить `manifest.json` и бинари на сервер, проверить доступность по URL

---

### Этап 3. Заглушка OTA в прошивке (только логирование)

**Задачи:**
- [ ] Создать `src/ota_update.h` и `src/ota_update.cpp` с заглушкой:
  - Парсит `update_ota` из ответа
  - Скачивает и парсит манифест
  - Логирует содержимое (URL, MD5, size)
  - Проверяет батарею
  - **НЕ выполняет** реальное обновление — только `LOG_INFO`
- [ ] Изменить `main.cpp` — добавить вызов `perform_ota_update()`

**Проверка:**
- [ ] Собрать: `platformio run --environment waterius_2` — компиляция без ошибок
- [ ] Проверить что размер прошивки не вырос критично (дельта < 10KB)
- [ ] Прошить ESP через USB
- [ ] В настройках Waterius указать URL PHP сниппета
- [ ] Дождаться пробуждения или нажать кнопку
- [ ] В Serial Monitor проверить логи:
  - `OTA: Manifest URL: https://...`
  - `OTA: firmware url=... md5=... size=...`
  - `OTA: filesystem url=... md5=... size=...`
  - `OTA: battery level: XX%`
  - `OTA: DRY RUN — skipping actual update`
- [ ] Проверить что без поля `update_ota` — OTA не запускается
- [ ] Проверить что невалидный URL манифеста → код ошибки 1 в логе

---

### Этап 4. Отправка ota_error в JSON

**Задачи:**
- [ ] Изменить `json.cpp` — добавить `ota_error` если `reserved9[0] != 0`
- [ ] Изменить `main.cpp` — обнулить `reserved9[0]` после успешной отправки

**Проверка:**
- [ ] Вручную записать код ошибки в `reserved9[0]` (тестовый код в заглушке)
- [ ] В Serial Monitor проверить что JSON содержит `"ota_error": N`
- [ ] Проверить в логе PHP сниппета что `ota_error` приходит
- [ ] При следующем пробуждении `ota_error` должен исчезнуть (обнулён)

---

### Этап 5. Реальное обновление firmware

**Задачи:**
- [ ] В `ota_update.cpp` заменить заглушку на реальный вызов `ESPhttpUpdate.update()`
- [ ] Добавить `extendWakeUp()` перед скачиванием
- [ ] Обновление FS пока **отключено** — только firmware

**Подготовка теста:**
- [ ] Собрать прошивку с версией `2.0.25-ota-test` (изменить `firmware_version` в platformio.ini)
- [ ] Запустить `plan/build_and_deploy.sh` — создать manifest.json, загрузить бинарь на сервер
- [ ] В PHP сниппете включить `$OTA_ENABLED = true`

**Проверка:**
- [ ] Прошить ESP текущей версией (`2.0.24`) через USB
- [ ] Дождаться пробуждения или нажать кнопку
- [ ] В Serial Monitor проверить:
  - `OTA: Downloading firmware...`
  - `OTA: Update success, rebooting`
- [ ] После перезагрузки проверить что `version_esp` = `2.0.25-ota-test`
- [ ] Отключить `$OTA_ENABLED` на сервере
- [ ] Проверить что следующее пробуждение — штатный цикл без OTA

**Тест ошибок:**
- [ ] Положить на сервер файл с неправильным MD5 в манифесте → `ota_error: 5`
- [ ] Указать несуществующий URL прошивки → `ota_error: 5`
- [ ] Указать `http://` вместо `https://` → `ota_error: 3`

---

### Этап 6. Обновление файловой системы LittleFS

**Задачи:**
- [ ] В `ota_update.cpp` добавить обработку блока `filesystem` из манифеста
- [ ] `ESPhttpUpdate.updateFS()` с `rebootOnUpdate(false)`
- [ ] Порядок: сначала FS, потом firmware

**Подготовка теста:**
- [ ] Изменить тестовый файл в `data/` (например добавить `<!-- OTA TEST -->` в `about.html`)
- [ ] Собрать FS: `platformio run --target buildfs --environment waterius_2`
- [ ] Обновить manifest.json с секцией `filesystem`
- [ ] Загрузить `nodemcuv2-X.X.X-fs.bin` на сервер

**Проверка:**
- [ ] Прошить ESP через USB (старая версия FS)
- [ ] Дождаться OTA
- [ ] После перезагрузки войти в setup mode (долгое нажатие кнопки)
- [ ] Открыть `192.168.4.1/about.html` — проверить что содержит `<!-- OTA TEST -->`
- [ ] Проверить что остальные файлы FS не повреждены (навигация по порталу)

**Тест: только FS без firmware:**
- [ ] Убрать секцию `firmware` из манифеста, оставить только `filesystem`
- [ ] Проверить что FS обновляется и ESP перезагружается (`ESP.restart()`)

---

### Этап 7. Проверка батареи

**Задачи:**
- [ ] Убедиться что проверка `voltage.get_battery_level() < 40` работает

**Проверка:**
- [ ] Временно установить `OTA_MIN_BATTERY_PERCENT = 101` (заведомо выше любого уровня)
- [ ] Проверить что OTA отклоняется: `ota_error: 7` в логе
- [ ] Вернуть `OTA_MIN_BATTERY_PERCENT = 40`
- [ ] С нормальными батарейками — OTA проходит

---

### Этап 8. Финальное тестирование

**Полный цикл:**
- [ ] Собрать релизную прошивку (новая версия)
- [ ] Запустить `build_and_deploy.sh`
- [ ] Прошить ESP старой версией через USB
- [ ] Включить OTA на сервере
- [ ] Дождаться штатного пробуждения (не кнопка) → OTA → проверить версию
- [ ] Отключить OTA на сервере
- [ ] Проверить 2-3 цикла штатной работы без OTA

**Тесты граничных условий:**
- [ ] Манифест без секции `filesystem` → обновляется только firmware
- [ ] Манифест без секции `firmware` → обновляется только FS
- [ ] Пустой манифест `{}` → `ota_error: 2`
- [ ] Сервер недоступен (выключить сниппет) → `ota_error: 1`
- [ ] Два обновления подряд (не отключать OTA) → второе тоже проходит

**Проверка энергопотребления:**
- [ ] Замерить время от пробуждения до сна: без OTA vs с OTA
- [ ] Убедиться что разница ~15-20 сек (не минуты)
