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

extern bool sync_ntp_time(const Settings &sett);
extern bool sync_ntp_time();
extern bool sync_ntp_time(const String &ntp_server_name);

extern String get_current_time();

extern bool is_valid_time(time_t time);

#endif
