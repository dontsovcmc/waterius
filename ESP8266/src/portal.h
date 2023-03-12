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

uint8_t get_auto_factor(uint32_t runtime_impulses, uint32_t impulses);
uint8_t get_factor(uint8_t combobox_factor, uint32_t runtime_impulses, uint32_t impulses, uint8_t cold_factor);


const char PARAM_WMAIL[]       PROGMEM = "wmail";
const char PARAM_WHOST[]       PROGMEM = "whost";
const char PARAM_MPERIOD[]       PROGMEM = "mperiod";
const char PARAM_BHOST[]       PROGMEM = "bhost";
const char PARAM_BKEY[]       PROGMEM = "bkey";
const char PARAM_BMAIL[]       PROGMEM = "bemail";
const char PARAM_BTITLE[]       PROGMEM = "btitle";
const char PARAM_BTEMPLATE[]       PROGMEM = "btemplate";
const char PARAM_MHOST[]       PROGMEM = "mhost";
const char PARAM_MPORT[]       PROGMEM = "mport";
const char PARAM_MLOGIN[]       PROGMEM = "mlogin";
const char PARAM_MPASSWORD[]       PROGMEM = "mpassword";
const char PARAM_MTOPIC[]       PROGMEM = "mtopic";
const char PARAM_MDAUTO[]       PROGMEM = "auto_discovery_checkbox";
const char PARAM_MDTOPIC[]       PROGMEM = "discovery_topic";
const char PARAM_IP[]       PROGMEM = "ip";
const char PARAM_GW[]       PROGMEM = "gw";
const char PARAM_SN[]       PROGMEM = "sn";
const char PARAM_NTP[]      PROGMEM = "ntp";
const char PARAM_FACTORCOLD[]       PROGMEM = "factorCold";
const char PARAM_FACTORHOT[]       PROGMEM = "factorHot";
const char PARAM_SERIALCOLD[]       PROGMEM = "serialCold";
const char PARAM_SERIALHOT[]       PROGMEM = "serialHot";
const char PARAM_CH0[]       PROGMEM = "ch0";
const char PARAM_CH1[]       PROGMEM = "ch1";

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
    AsyncCallbackWebHandler& on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest);

private:
    AsyncWebServer* server;
    uint32_t _delaydonesettings;
    bool _donesettings;
    bool _fail;
    void onGetRoot(AsyncWebServerRequest *request);
    void onGetScript(AsyncWebServerRequest *request);
    void onGetNetworks(AsyncWebServerRequest *request);
    void onGetConfig(AsyncWebServerRequest *request);
    void onGetStates(AsyncWebServerRequest *request);
    void onPostWifiSave(AsyncWebServerRequest *request);
    void onNotFound(AsyncWebServerRequest *request);
    void onExit(AsyncWebServerRequest *request);
    void onErase(AsyncWebServerRequest *request);
    bool SetParamStr(AsyncWebServerRequest *request, const char* param_name, char* dest, size_t size);
    bool UpdateParamStr(AsyncWebServerRequest *request, const char* param_name, char* dest, size_t size);
    bool SetParamIP(AsyncWebServerRequest *request, const char* param_name, uint32_t* dest);
    bool SetParamUInt(AsyncWebServerRequest *request, const char* param_name, uint16_t* dest);
    bool SetParamByte(AsyncWebServerRequest *request, const char* param_name, uint8_t* dest);
    bool SetParamFloat(AsyncWebServerRequest *request, const char* param_name, float* dest);

};

#endif