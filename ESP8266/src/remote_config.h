/**
 * @file remote_config.h
 * @brief Получение конфигурации устройства с удаленного сервера
 * @version 1.0
 * @date 2026-01-01
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef REMOTE_CONFIG_H_
#define REMOTE_CONFIG_H_

#include <Arduino.h>
#include "setup.h"
#include "master_i2c.h"

/**
 * @brief Получить и применить конфигурацию с удаленного сервера
 * 
 * Отправляет POST запрос на URL/cfg с ключом устройства,
 * получает настройки в формате JSON и применяет их.
 * 
 * @param url URL сервера (например, https://example.com)
 * @param key Ключ устройства для аутентификации
 * @param sett Структура настроек для обновления
 * @param data Данные от Attiny85 (для получения текущих типов счетчиков)
 * @param masterI2C Ссылка на объект для работы с Attiny85
 * @return true если настройки успешно получены и применены
 * @return false в случае ошибки
 */
bool fetch_and_apply_remote_config(const String &url, const char *key, Settings &sett, const AttinyData &data, MasterI2C &masterI2C);

#endif

