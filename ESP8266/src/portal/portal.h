/**
 * @file portal.h
 * @author neitri
 * @brief Модуль портала настройки асинхронным Web сервером
 * @version 0.2
 * @date 2022-02-27
 *
 * @copyright Copyright (c) 2023
 *
 * Клас работает совместно со статическими файлами расположенными в разделе файловой системы
 * Для добавления параметра необходимо отредактироват html докумет. Добавить необходимые элементы.
 * В методе onGetConfig необходимо добавить передачу значения параметра.
 * В методе onPostWifiSave необходимо обработать сохранение введенного значения.
 * В методе onGetStates можно обновлять параметр в реальном времени.
 *
 */
#ifndef Waterius_Portal_h
#define Waterius_Portal_h

#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "WebHandlerImpl.h"
#include <LittleFS.h>

class Portal
{
public:
    Portal();
    ~Portal();
    bool doneettings();
    void begin();
    void end();
    int8_t code;
    bool captivePortal(AsyncWebServerRequest *request);
    static bool isIp(String str);
    static String ipToString(uint32_t ip);
    AsyncCallbackWebHandler &on(const char *uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest);

    static bool SetParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size);
    static bool UpdateParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size);
    static bool SetParamIP(AsyncWebServerRequest *request, const char *param_name, uint32_t *dest);
    static bool SetParamUInt(AsyncWebServerRequest *request, const char *param_name, uint16_t *dest);
    static bool SetParamByte(AsyncWebServerRequest *request, const char *param_name, uint8_t *dest);
    static bool SetParamFloat(AsyncWebServerRequest *request, const char *param_name, float *dest);

    static bool SetParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size);
    static bool UpdateParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size);
    static bool SetParamIP(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint32_t *dest);
    static bool SetParamUInt(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint16_t *dest);
    static bool SetParamByte(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint8_t *dest);
    static bool SetParamFloat(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, float *dest);

private:
    AsyncWebServer *server;
    uint32_t _delaydonesettings;
    bool _donesettings;
    void onGetRoot(AsyncWebServerRequest *request);
    void onGetNetworks(AsyncWebServerRequest *request);
    void onNotFound(AsyncWebServerRequest *request);
    void onExit(AsyncWebServerRequest *request);
};

#endif