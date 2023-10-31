
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

/*
* Т.к. https://github.com/me-no-dev/ESPAsyncWebServer/issues/1249
*/
String replace_value(const String &var)
{
    String out(var);
    out.replace(F("%"), F("%%"));
    return out;
}

String get_counter_title(const uint8_t name)
{
    switch (name)
    {
        case CounterName::WATER_COLD:
            return F("Холодная вода");
        case CounterName::WATER_HOT:
            return F("Горячая вода");
        case CounterName::ELECTRO:
            return F("Электричество");
        case CounterName::GAS:
            return F("Газ");
        case CounterName::HEAT:
            return F("Тепло");
        case CounterName::PORTABLE_WATER:
            return F("Питьевая вода");
        case CounterName::OTHER:
        default:
            return F("Другой");
    }
}

String get_counter_img(const CounterName name)
{
    switch (name)
    {
        case CounterName::WATER_COLD:
            return F("meter-cold.png");
        case CounterName::WATER_HOT:
            return F("meter-hot.png");
        case CounterName::ELECTRO:
            return F("");
        case CounterName::GAS:
            return F("");
        case CounterName::HEAT:
            return F("");
        case CounterName::PORTABLE_WATER:
            return F("");
        case CounterName::OTHER:
        default:
            return F("");
    } 
}


String get_counter_instruction(const uint8_t name)
{
    switch (name)
    {
        case CounterName::WATER_COLD:
            return F("Спускайте воду в унитазе пока устройство не перенесёт вас на следующую страницу");
        case CounterName::WATER_HOT:
            return F("Откройте кран горячей воды пока устройство не перенесёт вас на следующую страницу");
        case CounterName::ELECTRO:
            return F("Включите электроприбор. После моргания светодиода должна открыться следующая страница. Если не открывается, значит некорректное подключение или счётчик не поддерживается.");
        case CounterName::GAS:
            return F("Приход импульса от газового счётчика долго ожидать, нажмите Пропустить и продолжите настройку.");
        case CounterName::HEAT:
            return F("Приход импульса от счётчика тепла долго ожидать, нажмите Пропустить и продолжите настройку.");
        case CounterName::PORTABLE_WATER:
            return F("Откройте кран питьевой воды пока устройство не перенесёт вас на следующую страницу");
        case CounterName::OTHER:
        default:
            return F("При приходе импульса от счётчика устройство перенесёт вас на следующую страницу");
    }
}

String processor(const String &var)
{
    if (var == FPSTR(PARAM_VERSION))
        return String(data.version);
    if (var == FPSTR(PARAM_VERSION_ESP))
        return String(sett.version);

    if (var == FPSTR(PARAM_WATERIUS_HOST))
        return replace_value(sett.waterius_host);
    if (var == FPSTR(PARAM_WATERIUS_EMAIL))
        return replace_value(sett.waterius_email);

    if (var == FPSTR(PARAM_BLYNK_KEY))
        return replace_value(sett.blynk_key);
    if (var == FPSTR(PARAM_BLYNK_HOST))
        return replace_value(sett.blynk_host);

    if (var == FPSTR(PARAM_HTTP_URL))
        return replace_value(sett.http_url);

    if (var == FPSTR(PARAM_MQTT_HOST))
        return replace_value(sett.mqtt_host);
    if (var == FPSTR(PARAM_MQTT_PORT))
        return String(sett.mqtt_port);
    if (var == FPSTR(PARAM_MQTT_LOGIN))
        return replace_value(sett.mqtt_login);
    if (var == FPSTR(PARAM_MQTT_PASSWORD))
        return replace_value(sett.mqtt_password);
    if (var == FPSTR(PARAM_MQTT_TOPIC))
        return replace_value(sett.mqtt_topic);

    if (var == FPSTR(PARAM_CHANNEL0_START))
        return String(sett.channel0_start);
    if (var == FPSTR(PARAM_CHANNEL1_START))
        return String(sett.channel1_start);

    if (var == FPSTR(PARAM_SERIAL0))
        return replace_value(sett.serial0);
    if (var == FPSTR(PARAM_SERIAL1))
        return replace_value(sett.serial1);

    if (var == FPSTR(PARAM_IP))
        return IPAddress(sett.ip).toString();
    if (var == FPSTR(PARAM_GATEWAY))
        return IPAddress(sett.gateway).toString();
    if (var == FPSTR(PARAM_MASK))
        return IPAddress(sett.mask).toString();
    if (var == FPSTR(PARAM_MAC_ADDRESS))
        return WiFi.macAddress();

    if (var == FPSTR(PARAM_WAKEUP_PER_MIN))
        return String(sett.wakeup_per_min);

    if (var == FPSTR(PARAM_MQTT_AUTO_DISCOVERY))
        return template_bool(sett.mqtt_auto_discovery);
    if (var == FPSTR(PARAM_MQTT_DISCOVERY_TOPIC))
        return replace_value(sett.mqtt_discovery_topic);

    if (var == FPSTR(PARAM_NTP_SERVER))
        return String(sett.ntp_server);

    if (var == FPSTR(PARAM_SSID))
        return replace_value(sett.wifi_ssid);
    if (var == FPSTR(PARAM_PASSWORD))
        return replace_value(sett.wifi_password);

    if (var == FPSTR(PARAM_WIFI_PHY_MODE))
        return String(sett.wifi_phy_mode);

    if (var == FPSTR(PARAM_COUNTER0_NAME))
        return String(sett.counter0_name);
    if (var == FPSTR(PARAM_COUNTER1_NAME))
        return String(sett.counter1_name);

    if (var == FPSTR(PARAM_COUNTER0_TITLE))
        return get_counter_title(sett.counter0_name);
    if (var == FPSTR(PARAM_COUNTER1_TITLE))
        return get_counter_title(sett.counter1_name);

    if (var == FPSTR(PARAM_COUNTER0_INSTRUCTION))
        return get_counter_instruction(sett.counter0_name);
    if (var == FPSTR(PARAM_COUNTER1_INSTRUCTION))
        return get_counter_instruction(sett.counter1_name);
    
    if (var == FPSTR(PARAM_COUNTER0_TYPE))
        return String(data.counter_type0);
    if (var == FPSTR(PARAM_COUNTER1_TYPE))
        return String(data.counter_type1);

    if (var == FPSTR(PARAM_FACTOR0))
        return String(sett.factor0);
    if (var == FPSTR(PARAM_FACTOR1))
        return String(sett.factor1);

    if (var == FPSTR(PARAM_WATERIUS_ON))
        return template_bool(sett.waterius_on);
    if (var == FPSTR(PARAM_HTTP_ON))
        return template_bool(sett.http_on);
    if (var == FPSTR(PARAM_MQTT_ON))
        return template_bool(sett.mqtt_on);
    if (var == FPSTR(PARAM_BLYNK_ON))
        return template_bool(sett.blynk_on);
    if (var == FPSTR(PARAM_DHCP_OFF))
        return template_bool(sett.dhcp_off);

    return String();
}

void onNotFound(AsyncWebServerRequest *request)
{
    LOG_INFO(F("onNotFound ") << request->url());
    if (captivePortal(request))
        return;
    request->send(404);
};

void onRoot(AsyncWebServerRequest *request)
{
    LOG_INFO(F("onRoot GET ") << request->url());
    if (captivePortal(request))
        return;

    if (sett.factor1 == AUTO_IMPULSE_FACTOR)
    {   
        // Первая настройка
        request->send(LittleFS, "/start.html", F("text/html"), false, processor);
        //request->send(LittleFS, "/index.html", F("text/html"), false, processor);
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
    if (sett.wifi_ssid[0])
    {
        LOG_INFO(F("Apply SSID:") << String(sett.wifi_ssid) << F(" from config"));
        struct station_config conf;
        conf.bssid_set = 0;
        memcpy(conf.ssid, sett.wifi_ssid, sizeof(conf.ssid));
        if (sett.wifi_password[0])
        {
            memcpy(conf.password, sett.wifi_password, sizeof(conf.password));
            LOG_INFO(F("Apply password from config"));
        }
        else
        {
            conf.password[0] = 0;
            LOG_INFO(F("No password in config"));
        }
        wifi_station_set_config(&conf);
    }
    else
    {
        LOG_INFO(F("No SSID saved in config"));
    }

    wifi_set_mode(WIFI_AP_STA);

    // TODO выбирать channel исходя из настроек.
    // Канал WiFi роутера к кому подсоединимся должен совпадать с каналом точки доступа ESP
    // https://bbs.espressif.com/viewtopic.php?t=324
    // TODO добавить пароль для интерфейса
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

    // TODO добавить .setLastModified( и  https://github.com/GyverLibs/buildTime/releases/tag/1.0
    server->serveStatic("/images/", LittleFS, "/images/").setCacheControl("max-age=600");
    server->serveStatic("/static/", LittleFS, "/static/").setCacheControl("max-age=600");

    // Об устройстве
    server->on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/about.html", F("text/html"), false, processor); });

    server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/favicon.ico", F("image/x-icon")); });

    // Завершение настройки (wizard)
    server->on("/finish.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/finish.html", F("text/html"), false, processor); });

    // Главная страница
    server->on("/", HTTP_GET, onRoot).setFilter(ON_AP_FILTER);
    server->on("/fwlink", HTTP_GET, onRoot).setFilter(ON_AP_FILTER); // Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.

    server->on("/logs.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/logs.html", F("text/html"), false, processor); });

    // Сброс к заводским настройкам
    server->on("/reset.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/reset.html", F("text/html"), false, processor); });

    // Тип синего счётчика
    server->on("/setup_blue_type.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_blue_type.html", F("text/html"), false, processor); });

    // Детектирование синего счётчика (только Холодная и Горячая вода)
    server->on("/setup_blue_water.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_blue_water.html", F("text/html"), false, processor); });

    // Параметры синего счётчика
    server->on("/setup_blue.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_blue.html", F("text/html"), false, processor); });

    // Тип красного счётчика
    server->on("/setup_red_type.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_red_type.html", F("text/html"), false, processor); });

    // Детектирование красного счётчика (только Холодная и Горячая вода)
    server->on("/setup_red_water.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_red_water.html", F("text/html"), false, processor); });

    // Параметры красного счётчика
    server->on("/setup_red.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_red.htm", F("text/html"), false, processor); });

    // Отправка показаний 
    server->on("/setup_send.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_send.html", F("text/html"), false, processor); });

    // Первая настройка
    server->on("/start.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/start.html", F("text/html"), false, processor); });

    server->on("/waterius_logs.txt", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/waterius_logs.txt", F("text/plain")); });

    // Процесс подключения к Wi-fi
    server->on("/wifi_connect.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_connect.html", F("text/html"), false, processor); });

    // Список Wi-Fi сетей
    server->on("/wifi_list.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_list.html", F("text/html"), false, processor); });

    // Ввод пароля от Wi-Fi сети
    server->on("/wifi_password.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_password.html", F("text/html"), false, processor); });

    // Настройки Wi-Fi, NTP
    server->on("/wifi_settings.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_settings.html", F("text/html"), false, processor); });

    // Файл характеристик всех найденных Wi-Fi сетей 
    server->on("/ssid.txt", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/ssid.txt", F("text/plain")); });

    server->onNotFound(onNotFound);

    /*API*/
    server->on("/api/networks", HTTP_GET, onGetApiNetworks);  // Список Wi-Fi сетей (из wifi_list.html)
    server->on("/api/init_connect", HTTP_POST, onPostApiInitConnect); // Сохраняем настройки Wi-Fi + redirect: /api/connect
    server->on("/api/call_connect", HTTP_GET, onGetApiCallConnect);  // Поднимаем флаг старта подключения и redirect в wifi_connect.html
    server->on("/api/connect_status", HTTP_GET, onGetApiConnectStatus); // Статус подключения (из wifi_connect.html)
    server->on("/api/setup", HTTP_POST, onPostApiSetup); // Сохраняем настройки
    server->on("/api/set_counter_type/0", HTTP_POST, onPostApiSetCounterType0); // Сохраняем тип счётчика и переносим на страницу настройки
    server->on("/api/set_counter_type/1", HTTP_POST, onPostApiSetCounterType1); // Сохраняем тип счётчика и переносим на страницу настройки
    server->on("/api/main_status", HTTP_GET, onGetApiMainStatus); // Информационные сообщения на главной странице
    server->on("/api/status/0", HTTP_GET, onGetApiStatus0);  // Статус 0-го входа (ХВС)  (из setup_cold_welcome.html)
    server->on("/api/status/1", HTTP_GET, onGetApiStatus1);  // Статус 1-го входа (ГВС)  (из setup_cold_welcome.html)
    server->on("/api/turnoff", HTTP_GET, onGetApiTurnOff);   // Выйти из режима настройки
    server->on("/api/reset", HTTP_POST, onPostApiReset);     // Сброс к заводским настройкам

    server->begin();

    LOG_INFO(F("HTTP server started"));

    // Начинаем сканирование Wi-Fi сетей
    LOG_INFO(F("Start scan Wi-Fi networks"));
    WiFi.scanNetworks(true);

    uint16_t start = millis();
    while (!exit_portal_flag && ((millis() - start) / 1000) < SETUP_TIME_SEC)
    {
        dns->processNextRequest();
        yield();

        if (start_connect_flag)
        {
            wifi_connect(sett, WIFI_AP_STA);
            start_connect_flag = false;
        }
        if (factory_reset_flag)
        {
            factory_reset(sett);
        }
    }

    if (((millis() - start) / 1000) > SETUP_TIME_SEC)
    {
        LOG_ERROR(F("Portal setup time is over"));
    }

    LOG_INFO(F("Shutdown HTTP and DNS servers"));

    server->end();
    dns->stop();
    delete server;
    delete dns;
};
