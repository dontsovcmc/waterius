/**
 * @file sync_time.h
 * @author vzagorovskiy
 * @brief Модуль синхронизации системного времени с NTP серверами
 * @version 0.1
 * @date 2023-02-08
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef SYNCTIME_h_
#define SYNCTIME_h_

#include <Arduino.h>
#include "setup.h"
#include "time.h"
#include "core/timekeeping.h"

/*
Синхронизировать время, если пришёл срок (#357).

Между синхронизациями часы ставятся оценкой по числу проспанных периодов.
Возвращает true, только если время действительно получено от сервера:
по этому признаку решается, можно ли пересчитывать поправку частоты attiny.
*/
extern bool maybe_sync_time(Settings &sett);

extern bool sync_ntp_time(const Settings &sett);
extern bool sync_ntp_time(const time_t known_good = START_VALID_TIME);
extern bool sync_ntp_time(const String &ntp_server_name, const time_t known_good = START_VALID_TIME, const int attempt = 1);

extern String get_current_time();

// Возвращает 0, пока время неизвестно. Нужна успешная синхронизация с NTP
// или сохранённая дата предыдущей синхронизации. Начальная дата из прошивки
// для проверки сертификатов не подходит.
extern time_t get_tls_time();

// is_valid_time() объявлена в core/timekeeping.h

#endif
