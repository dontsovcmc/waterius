# План доработки OTA v2

## Что сделано

| # | Задача | Файлы |
|---|--------|-------|
| 1 | Убрать манифест, парсить OTA из JSON-ответа | ota_update.cpp, ota_update.h, main.cpp |
| 2 | Удалить ota_manifest.h и test_manifest.cpp | ota_manifest.h, test_manifest.cpp |
| 3 | Упростить проверку батареи (только voltage, 3 замера/100мс) | ota_update.cpp, ota_update.h |
| 4 | Перенумеровать коды ошибок (5 вместо 8) | ota_update.h |
| 5 | Обновить скрипт сборки (без manifest.json) | scripts/build_and_deploy.sh |
| 6 | Обновить doc/ota.md | doc/ota.md |
| 7 | Извлечь parse_ota_params() в ota_parse.h, native-тесты | ota_parse.h, ota_update.cpp, test_url_validation.cpp |
| 8 | Обновить PHP-сниппет под OTA v2 | doc/waterius_ota_snippet.php |
| 9 | Тестирование на устройстве | — |

### Ключевые изменения

**Протокол:** `{"update_ota": "url"}` → `{"ota": {"firmware": {...}, "filesystem": {...}}}`. Убран отдельный HTTP-запрос на манифест — экономия ~0.6с и ~15 КБ RAM.

**Проверка батареи:** убрана нестабильная проверка `battery% < 40%`. Вместо этого — 3 измерения напряжения с шагом 100мс, среднее сравнивается с порогом 3300 мВ.

**Коды ошибок:** 0=NONE, 1=PARSE, 2=FS_UPDATE, 3=FW_UPDATE, 4=LOW_BATTERY. Убраны DOWNLOAD, INVALID_URL, TIMEOUT (не используются).

**HTTPS:** оставлен (WiFiClientSecure + setInsecure). Памяти хватает, защита от пассивного перехвата.

**PHP-сниппет:** обновлён под v2 — отдаёт секцию `ota` вместо `update_ota`.

**Native-тесты:** 11 тестов (googletest) для parse_ota_params() — парсинг JSON, валидация полей, коды ошибок.

---

## Результаты тестирования на устройстве

**Дата:** 2026-03-16
**Устройство:** Waterius-2 (ESP-12F, 4MB flash), ChipId: 682bdb
**Питание:** USB (4732 mV)
**WiFi:** ZLINKS, 802.11n, канал 7, RSSI -49 dBm
**Сервер:** WordPress + Code Snippets на us.pstd.ru

### Тест: полный OTA (firmware + filesystem), 2.0.24 → 2.0.25

| Этап | Время | Результат |
|------|-------|-----------|
| WiFi подключение | ~3.8с | OK |
| NTP синхронизация | ~0.05с | OK |
| HTTP POST → сервер вернул `ota` | ~1.1с | OK, JSON с firmware + filesystem |
| Проверка батареи | — | Пропущена (USB: 4732 mV > 4600) |
| Парсинг OTA JSON | мгновенно | OK, url/md5/size распознаны |
| Скачивание filesystem (1024 KB) | ~47с (~22 KB/с) | OK |
| Скачивание firmware (633 KB) | ~34с (~19 KB/с) | OK |
| Перезагрузка (eboot copy) | ~3.8с | OK |
| **Полный цикл OTA** | **~88с** | **OK** |

### Проверено

- Сервер вернул `{"ota": {"firmware": {...}, "filesystem": {...}}}` — ESP распарсил корректно
- USB-питание определено (4732 mV > 4600 mV) — проверка батареи пропущена
- Filesystem обновлён, затем firmware — порядок правильный
- После рестарта: `Firmware ver: 2.0.25` — версия обновилась
- EEPROM/Settings сохранились (WiFi, ключи, показания — всё на месте)
- Второй запрос к серверу → `{}` (version_esp=2.0.25 ≠ target 2.0.24) — OTA не зациклился
- Устройство нормально ушло в сон после OTA

### Native-тесты (googletest)

```
platformio test --environment native
11 tests from 2 test suites — all PASSED
```

Тесты: FirmwareAndFilesystem, FirmwareOnly, FilesystemOnly, EmptyObject, FirmwareMissingUrl, FirmwareMissingMd5, FilesystemMissingUrl, FilesystemMissingMd5, SizeDefaultsToZero, FirmwareNotObject, OtaErrorCodes.Values.

---

## Варианты на будущее

### Dual-Partition OTA (A/B)

Сейчас ESP8266 использует staging area: eboot копирует прошивку поверх текущей (~1-3 сек). Dual-partition устраняет риск brick при сбое питания, но требует rboot, уменьшение LittleFS до 512 KB и ограничение прошивки до ~1.2 MB.

**Статус:** отложено. `extendWakeUp()` минимизирует риск сбоя питания.
