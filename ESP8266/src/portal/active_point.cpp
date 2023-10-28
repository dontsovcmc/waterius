
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
#include "active_point_api.h"
#include "active_point.h"


#define SETUP_TIME_SEC 600UL // На какое время Attiny включает ESP (файл Attiny85\src\Setup.h)

bool exit_portal_flag = false;
bool start_connect_flag = false;
bool factory_reset_flag = false;

extern SlaveData data;
extern MasterI2C masterI2C;
extern Settings sett;
extern CalculatedData cdata;

String template_bool(const uint8_t value)
{
    if (value > 0) 
    {
        return String(F("'value=\"1\" checked"));
    }
    return String();
}

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
        return template_bool(sett.mqtt_auto_discovery);
    if(var == FPSTR(PARAM_MQTT_DISCOVERY_TOPIC))
        return String(sett.mqtt_discovery_topic);

    if(var == FPSTR(PARAM_NTP_SERVER))
        return String(sett.ntp_server);

    if(var == FPSTR(PARAM_SSID))
        return String(sett.wifi_ssid);
    if(var == FPSTR(PARAM_PASSWORD))
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
        return template_bool(sett.waterius_on);
    if(var == FPSTR(PARAM_HTTP_ON))
        return template_bool(sett.http_on);
    if(var == FPSTR(PARAM_MQTT_ON))
        return template_bool(sett.mqtt_on);
    if(var == FPSTR(PARAM_BLYNK_ON))
        return template_bool(sett.blynk_on);
    if(var == FPSTR(PARAM_DHCP_OFF))
        return template_bool(sett.dhcp_off);

    return String();
}


void onNotFound(AsyncWebServerRequest *request)
{
    LOG_INFO(F("onNotFound ") << request->url());
    if(captivePortal(request))
        return;
    request->send(404);
};


void onRoot(AsyncWebServerRequest *request)
{
    LOG_INFO(F("onRoot GET ") << request->url());
    if(captivePortal(request))
        return;

    if (sett.factor1 == AUTO_IMPULSE_FACTOR) 
    {   
        //TODO
        //request->send(LittleFS, "/start.html", F("text/html"), false, processor);
        request->send(LittleFS, "/index.html", F("text/html"), false, processor);  
    } 
    else 
    {
        request->send(LittleFS, "/index.html", F("text/html"), false, processor);
    }
}


void start_active_point(Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    if (!LittleFS.begin())
    {
        LOG_INFO(F("An Error has occurred while mounting LittleFS"));
        return;
    }
    LOG_INFO(F("LittleFS mounted"));

    // Если настройки есть в конфиге то присваиваем их 
    if (sett.wifi_ssid[0]) {
        LOG_INFO(F("Apply SSID:") << String(sett.wifi_ssid) << F(" from config"));
        struct station_config conf;
        conf.bssid_set = 0;
        memcpy(conf.ssid, sett.wifi_ssid, sizeof(conf.ssid));
        if (sett.wifi_password[0]) {
            memcpy(conf.password, sett.wifi_password, sizeof(conf.password));
            LOG_INFO(F("Apply password from config"));
        } else {
            conf.password[0] = 0;
            LOG_INFO(F("No password in config"));
        }    
        wifi_station_set_config(&conf);
    } else {
        LOG_INFO(F("No SSID saved in config"));
    }
    
    wifi_set_mode(WIFI_AP_STA);

    //TODO выбирать channel исходя из настроек. 
    //Канал WiFi роутера к кому подсоединимся должен совпадать с каналом точки доступа ESP
    //https://bbs.espressif.com/viewtopic.php?t=324
    //TODO добавить пароль для интерфейса
    if (!WiFi.softAP(get_ap_name(), "", sett.wifi_channel, 0, 4)) 
    {
        LOG_ERROR(F("AP started failed"));
        return;
    }
  
    delay(500);

    LOG_INFO(F("AP started on channel=") << sett.wifi_channel << F(" , ssid=") << get_ap_name());
    LOG_INFO(F("IP: ") << WiFi.softAPIP());
    
    LOG_INFO(F("Start DNS server"));
    DNSServer *dns = new DNSServer();
    dns->start(53, "*", WiFi.softAPIP());

    LOG_INFO(F("DNS server started"));
    
    LOG_INFO(F("Start HTTP server"));
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

    server->on("/wifi_connect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/wifi_connect.html", F("text/html"), false, processor);
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

    /*Debug*/
    server->on("/ssid.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/ssid.txt", F("text/plain"));
    });

    server->onNotFound(onNotFound);

    /*API*/
    server->on("/api/networks", HTTP_GET, onGetApiNetworks);
    server->on("/api/connect", HTTP_POST, onPostApiConnect);
    server->on("/api/connect_status", HTTP_GET, onGetApiConnectStatus);
    server->on("/api/setup", HTTP_POST, onPostApiSetup);
    server->on("/api/main_status", HTTP_GET, onGetApiMainStatus);
    server->on("/api/status/0", HTTP_GET, onGetApiStatus0);
    server->on("/api/status/1", HTTP_GET, onGetApiStatus1);
    server->on("/api/turnoff", HTTP_GET, onGetApiTurnOff);
    server->on("/api/reset", HTTP_POST, onPostApiReset);

    server->begin();

    LOG_INFO(F("HTTP server started"));

    //Начинаем сканирование Wi-Fi сетей
    LOG_INFO(F("Start scan Wi-Fi networks"));
    WiFi.scanNetworks(true);

    uint16_t start = millis();
    while (!exit_portal_flag && ((millis() - start) / 1000) < SETUP_TIME_SEC)
    {
        dns->processNextRequest();
        yield();
        
        if (start_connect_flag) {
            wifi_connect(sett, WIFI_AP_STA);
            start_connect_flag = false;
        }
        if (factory_reset_flag) {
            factory_reset(sett);
        }
    }

    if (((millis() - start) / 1000) > SETUP_TIME_SEC) {
        LOG_ERROR(F("Portal setup time is over"));
    }

    LOG_INFO(F("Shutdown HTTP and DNS servers"));

    server->end();
    dns->stop();
    delete server;
    delete dns;
};
