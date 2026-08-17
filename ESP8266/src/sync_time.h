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

extern bool sync_ntp_time(const Settings &sett);
extern bool sync_ntp_time(const time_t known_good = START_VALID_TIME);
extern bool sync_ntp_time(const String &ntp_server_name, const time_t known_good = START_VALID_TIME);

extern String get_current_time();

// is_valid_time() объявлена в core/timekeeping.h

#endif
