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
void save_param(AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj);
void save_bool_param(AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj);
void save_param(AsyncWebParameter *p, float &v, JsonObject &errorsObj);
void save_ip_param(AsyncWebParameter *p, uint32_t &v, JsonObject &errorsObj);

bool find_wizard_param(AsyncWebServerRequest *request);
void applySettings(AsyncWebServerRequest *request, JsonObject &errorsObj);

bool captivePortal(AsyncWebServerRequest *request);
void onPostApiInitConnect(AsyncWebServerRequest *request);
void onGetApiCallConnect(AsyncWebServerRequest *request);
void onGetApiConnectStatus(AsyncWebServerRequest *request);
void onPostApiSetCounterType0(AsyncWebServerRequest *request); 
void onPostApiSetCounterType1(AsyncWebServerRequest *request); 
void onPostApiSetCounterType(AsyncWebServerRequest *request, const uint8_t index);
void onGetApiNetworks(AsyncWebServerRequest *request);
void onGetApiMainStatus(AsyncWebServerRequest *request);
void onGetApiStatus0(AsyncWebServerRequest *request);
void onGetApiStatus1(AsyncWebServerRequest *request);
void onGetApiStatus(AsyncWebServerRequest *request, const int index);
void onPostApiSetup(AsyncWebServerRequest *request);
void onGetApiTurnOff(AsyncWebServerRequest *request);
void onPostApiReset(AsyncWebServerRequest *request);

#endif