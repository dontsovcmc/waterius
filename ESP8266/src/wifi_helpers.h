/**
 * @file wifi_helpers.h
 * @brief Функции по работе c wifi
 * @version 0.1
 * @date 2023-02-11
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef WIFI_HELPERS_H_
#define WIFI_HELPERS_H_

#include <ESP8266WiFi.h>
#include "setup.h"

extern bool wifi_connect(Settings &sett, WiFiMode_t wifi_mode = WIFI_STA);
extern void wifi_begin(Settings &sett, WiFiMode_t wifi_mode);
extern void wifi_set_mode(WiFiMode_t wifi_mode);
extern void wifi_shutdown();
extern String wifi_mode();

extern void write_ssid_to_file();

#endif