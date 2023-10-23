
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "WebHandlerImpl.h"
#include <IPAddress.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "AsyncJson.h"
#include "setup.h"
#include "Logging.h"
#include "master_i2c.h"
#include "utils.h"
#include "config.h"
#include "wifi_helpers.h"
#include "resources.h"
#include "param_helpers.h"
#include "active_point_api.h"
#include "active_point.h"


#define SETUP_TIME_SEC 600UL // На какое время Attiny включает ESP (файл Attiny85\src\Setup.h)

bool exit_portal = false;

extern SlaveData data;
extern MasterI2C masterI2C;
extern Settings sett;
extern CalculatedData cdata;


String processor(const String& var)
{
    if(var == FPSTR(PARAM_VERSION))
        return String(data.version);
    if(var == FPSTR(PARAM_VERSION_ESP))
        return String(sett.version);

    if(var == FPSTR(PARAM_WATERIUS_HOST))
        return String(sett.waterius_host);
    if(var == FPSTR(PARAM_WATERIUS_EMAIL))
        return String(sett.waterius_email);

    if(var == FPSTR(PARAM_BLYNK_KEY))
        return String(sett.blynk_key);
    if(var == FPSTR(PARAM_BLYNK_HOST))
        return String(sett.blynk_host);

    if(var == FPSTR(PARAM_HTTP_URL))
        return String(sett.http_url);

    if(var == FPSTR(PARAM_MQTT_HOST))
        return String(sett.mqtt_host);
    if(var == FPSTR(PARAM_MQTT_PORT))
        return String(sett.mqtt_port);
    if(var == FPSTR(PARAM_MQTT_LOGIN))
        return String(sett.mqtt_login);
    if(var == FPSTR(PARAM_MQTT_PASSWORD))
        return String(sett.mqtt_password);
    if(var == FPSTR(PARAM_MQTT_TOPIC))
        return String(sett.mqtt_topic);

    if(var == FPSTR(PARAM_CHANNEL0_START))
        return String(sett.channel0_start);
    if(var == FPSTR(PARAM_CHANNEL1_START))
        return String(sett.channel1_start);

    if(var == FPSTR(PARAM_SERIAL0))
        return String(sett.serial0);
    if(var == FPSTR(PARAM_SERIAL1))
        return String(sett.serial1);

    if(var == FPSTR(PARAM_IP))
        return IPAddress(sett.ip).toString();
    if(var == FPSTR(PARAM_GATEWAY))
        return IPAddress(sett.gateway).toString();
    if(var == FPSTR(PARAM_MASK))
        return IPAddress(sett.mask).toString();
    if(var == FPSTR(PARAM_MAC_ADDRESS))
        return WiFi.macAddress();

    if(var == FPSTR(PARAM_WAKEUP_PER_MIN))
        return String(sett.wakeup_per_min);

    if(var == FPSTR(PARAM_MQTT_AUTO_DISCOVERY))
        return String(sett.mqtt_auto_discovery);
    if(var == FPSTR(PARAM_MQTT_DISCOVERY_TOPIC))
        return String(sett.mqtt_discovery_topic);

    if(var == FPSTR(PARAM_NTP_SERVER))
        return String(sett.ntp_server);

    if(var == FPSTR(PARAM_WIFI_SSID))
        return String(sett.wifi_ssid);
    if(var == FPSTR(PARAM_WIFI_PASSWORD))
        return String(sett.wifi_password);

    if(var == FPSTR(PARAM_WIFI_PHY_MODE))
        return String(sett.wifi_phy_mode);

    if(var == FPSTR(PARAM_COUNTER0_NAME))
        return String(sett.counter0_name);
    if(var == FPSTR(PARAM_COUNTER1_NAME))
        return String(sett.counter1_name);

    if(var == FPSTR(PARAM_COUNTER0_TYPE))
        return String(data.counter_type0);
    if(var == FPSTR(PARAM_COUNTER1_TYPE))
        return String(data.counter_type1);

    if(var == FPSTR(PARAM_FACTOR0))
        return String(sett.factor0);
    if(var == FPSTR(PARAM_FACTOR1))
        return String(sett.factor1);

    if(var == FPSTR(PARAM_WATERIUS_ON))
        return String(sett.waterius_on);
    if(var == FPSTR(PARAM_HTTP_ON))
        return String(sett.http_on);
    if(var == FPSTR(PARAM_MQTT_ON))
        return String(sett.mqtt_on);
    if(var == FPSTR(PARAM_BLYNK_ON))
        return String(sett.blynk_on);
    if(var == FPSTR(PARAM_DHCP_ON))
        return String(sett.dhcp_on);

    return String();
}


void onNotFound(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer 404 ") << request->host()<< request->url());
    if(captivePortal(request))
        return;
    request->send(404);
};


void onRoot(AsyncWebServerRequest *request)
{
    LOG_INFO(F("GET /") << request->host() << request->url());
    if (captivePortal(request))
        return;      

    //request->send(LittleFS, "/start.html", String(), false, processor);
    request->send(LittleFS, "/index.html", String(), false, processor);
}


void start_active_point(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    // Если настройки есть в конфиге то присваиваем их 
    if (sett.wifi_ssid[0]) {
        struct station_config conf;
        conf.bssid_set = 0;
        memcpy(conf.ssid, sett.wifi_ssid, sizeof(conf.ssid));
        if (sett.wifi_password[0]) {
            memcpy(conf.password, sett.wifi_password, sizeof(conf.password));
        } else {
            conf.password[0] = 0;
        }    
        wifi_station_set_config(&conf);
    }

    DNSServer *dns = new DNSServer();
    dns->start(53, "*", WiFi.softAPIP());
    
    AsyncWebServer *server = new AsyncWebServer(80);
    
    //TODO добавить .setLastModified( и  https://github.com/GyverLibs/buildTime/releases/tag/1.0
    server->serveStatic("/images/", LittleFS, "/images/");

    server->on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/about.html", F("text/html"), false, processor);
    });

    server->on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css", F("text/css"));
    });
    server->on("/common.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/common.js", F("text/javascript"));
    });
    server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/favicon.ico", F("image/x-icon"));
    });

    server->on("/finish.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/finish.html", F("text/html"), false, processor);
    });

    server->on("/", HTTP_GET, onRoot);
    server->on("/fwlink", HTTP_GET, onRoot);  //captive

    server->on("/logs.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/logs.html", F("text/html"), false, processor);
    });

    server->on("/reset.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/reset.html", F("text/html"), false, processor);
    });

    server->on("/setup_cold_welcome.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/setup_cold_welcome.html", F("text/html"), false, processor);
    });

    server->on("/setup_cold.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/setup_cold.html", F("text/html"), false, processor);
    });

    server->on("/setup_hot_welcome.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/setup_hot_welcome.html", F("text/html"), false, processor);
    });

    server->on("/setup_hot.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/setup_hot.htm", F("text/html"), false, processor);
    });

    server->on("/setup_send.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/setup_send.html", F("text/html"), false, processor);
    });

    server->on("/start.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/start.html", F("text/html"), false, processor);
    });

    server->on("/waterius_logs.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/waterius_logs.txt", F("text/plain"));
    });

    server->on("/wifi_list.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/wifi_list.html", F("text/html"), false, processor);
    });

    server->on("/wifi_password.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/wifi_password.html", F("text/html"), false, processor);
    });

    server->on("/wifi_settings.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/wifi_settings.html", F("text/html"), false, processor);
    });

    server->onNotFound(onNotFound);

    /*API*/
    server->on("/api/networks", HTTP_GET, onGetApiNetworks);
    server->on("/api/connect", HTTP_GET, onGetApiConnect);
    server->on("/api/main_status", HTTP_GET, onGetApiMainStatus);
    server->on("/api/status/0", HTTP_GET, onGetApiStatus0);
    server->on("/api/status/1", HTTP_GET, onGetApiStatus1);
    server->on("/api/turnoff", HTTP_GET, onGetApiTurnOff);
    server->on("/api/reset", HTTP_GET, onGetApiReset);

    server->begin();

    //Начинаем сканирование Wi-Fi сетей
    WiFi.scanNetworks(true);

    uint16_t start = millis();
    while (!exit_portal && millis() - start < SETUP_TIME_SEC)
    {
        dns->processNextRequest();
        yield();
    }
    server->end();
    dns->stop();
    delete server;
    delete dns;
};
