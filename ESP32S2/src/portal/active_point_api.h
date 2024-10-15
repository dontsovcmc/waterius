/**
 * @file active_point_api.h
 * @brief Хэндлеры веб сервера настроки ватериуса
 * @version 0.1
 * @date 2023-10-22
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef ACTIVE_POINT_API_h_
#define ACTIVE_POINT_API_h_

#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>

void save_param(AsyncWebParameter *p, char *dest, size_t size, JsonObject &errorsObj, bool required = true);
void save_param(AsyncWebParameter *p, uint16_t &v, JsonObject &errorsObj);
void save_param(AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj, const bool zero_ok = false);
void save_bool_param(AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj);
void save_param(AsyncWebParameter *p, float &v, JsonObject &errorsObj);
void save_ip_param(AsyncWebParameter *p, uint32_t &v, JsonObject &errorsObj);

bool find_wizard_param(AsyncWebServerRequest *request);
void applyInputSettings(AsyncWebServerRequest *request, JsonObject &errorsObj, const uint8_t input);
void applySettings(AsyncWebServerRequest *request, JsonObject &errorsObj);

void post_api_save_connect(AsyncWebServerRequest *request);
void get_api_start_connect(AsyncWebServerRequest *request);
void get_api_connect_status(AsyncWebServerRequest *request);
void post_api_save_input_type(AsyncWebServerRequest *request);
void get_api_networks(AsyncWebServerRequest *request);
void get_api_main_status(AsyncWebServerRequest *request);
void get_api_status_0(AsyncWebServerRequest *request);
void get_api_status_1(AsyncWebServerRequest *request);
void get_api_status(AsyncWebServerRequest *request, const int index);
void post_api_save(AsyncWebServerRequest *request);
void get_api_turnoff(AsyncWebServerRequest *request);
void post_api_reset(AsyncWebServerRequest *request);

#endif