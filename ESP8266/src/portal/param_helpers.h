/**
 * @file portal.h
 * @author neitri
 * @brief 
 * @version 0.2
 * @date 2022-02-27
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef Waterius_Portal_h
#define Waterius_Portal_h

#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"

bool SetParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size);
bool UpdateParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size);
bool SetParamIP(AsyncWebServerRequest *request, const char *param_name, uint32_t *dest);
bool SetParamUInt(AsyncWebServerRequest *request, const char *param_name, uint16_t *dest);
bool SetParamByte(AsyncWebServerRequest *request, const char *param_name, uint8_t *dest);
bool SetParamFloat(AsyncWebServerRequest *request, const char *param_name, float *dest);

bool SetParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size);
bool UpdateParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size);
bool SetParamIP(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint32_t *dest);
bool SetParamUInt(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint16_t *dest);
bool SetParamByte(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint8_t *dest);
bool SetParamFloat(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, float *dest);

#endif