
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

bool exit_portal = false;


SlaveData runtime_data;
extern SlaveData data;
extern MasterI2C masterI2C;
extern Settings sett;
extern CalculatedData cdata;

#define SETUP_TIME_SEC 600UL // На какое время Attiny включает ESP (файл Attiny85\src\Setup.h)
#define IMPULS_LIMIT_1 3 // Если пришло импульсов меньше 3, то перед нами 10л/имп. Если больше, то 1л/имп.

uint8_t get_auto_factor(uint32_t runtime_impulses, uint32_t impulses)
{
    return (runtime_impulses - impulses <= IMPULS_LIMIT_1) ? 10 : 1;
}

uint8_t get_factor(uint8_t combobox_factor, uint32_t runtime_impulses, uint32_t impulses, uint8_t cold_factor)
{

    switch (combobox_factor)
    {
    case AUTO_IMPULSE_FACTOR:
        return get_auto_factor(runtime_impulses, impulses);
    case AS_COLD_CHANNEL:
        return cold_factor;
    default:
        return combobox_factor; // 1, 10, 100
    }
}


bool captivePortal(AsyncWebServerRequest *request)
{
    if(IPAddress::isValid(request->host()))
        return false;
    String url= String("http://") + IPAddress(request->client()->getLocalAddress()).toString();
    LOG_INFO(F("Request redirected to captive portal ") << url);
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", url);
    request->send(response);
    return true;
}

void onNotFound(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer 404 ") << request->host()<< request->url());
    if(captivePortal(request))
        return;
    request->send(404);
};

void onExit(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer onExit GET ") << request->host()<< request->url());
    if(captivePortal(request))
        return;    
    exit_portal = true;
    
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "");
    request->send(response);
};

void onRoot(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer onGetRoot GET ") << request->host() << request->url());
    if (captivePortal(request))
        return;
    request->send(LittleFS, "/index.html");
}

void onGetNetworks(AsyncWebServerRequest *request)
{
    if (captivePortal(request))
        return;
    LOG_INFO(F("AsyncWebServer onGetNetworks GET ") << request->host() << request->url());
    
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED)
    {
        WiFi.scanNetworks(true);
        request->send(200, "", F("Wi-Fi сети не найдены. Перезагрузите страницу для повторного сканирования."));
    }
    else if (n)
    {
        String networks;
        networks.reserve(1000);

        for (int i = 0; i < n; ++i)
        {
            LOG_INFO(WiFi.SSID(i) << " " << WiFi.RSSI(i));
            networks += F("<label class='radcnt' onclick='c(this)'>");
            networks += String(WiFi.SSID(i));
            networks += F("<input type='radio' name='n'><span class='rmrk'></span><div role='img' class='q q-");
            networks += String(int(round(map(WiFi.RSSI(i), -100, -50, 1, 4))));
            if (WiFi.encryptionType(WiFi.encryptionType(i)) != ENC_TYPE_NONE)
            {
                networks += F(" l");
            }
            networks += F("'></div></label>");
        }
        WiFi.scanDelete();
        networks += F("<br/>");
        request->send(200, "", networks);
    }
};

void onGetStates(AsyncWebServerRequest *request)
{
    if (captivePortal(request))
        return;
    DynamicJsonDocument json_doc(JSON_SMALL_STATIC_MSG_BUFFER);
    JsonObject root = json_doc.to<JsonObject>();

    if (masterI2C.getSlaveData(runtime_data))
    {
        uint32_t delta0 = runtime_data.impulses0 - data.impulses0;
        uint32_t delta1 = runtime_data.impulses1 - data.impulses1;

        if (sett.factor0 != AS_COLD_CHANNEL 
         && sett.factor0 != AUTO_IMPULSE_FACTOR) 
        {   // повторная настройка
            root[F("state0good")] = F("Состояние неизвестно");
            root[F("state0bad")] = F("");
        } 
        if (delta0 > 0)
        {
            root[F("state0good")] = F("Подключён");
            root[F("state0bad")] = F("");
        } 

        int factor1;
        if (sett.factor1 == AUTO_IMPULSE_FACTOR)
        {
            factor1 = get_auto_factor(runtime_data.impulses1, data.impulses1);
        }
        else  // повторная настройка
        {
            factor1 = sett.factor1;
            root[F("state1good")] = F("Состояние неизвестно");
            root[F("state1bad")] = F("");
        }

        if (delta1 > 0)
        {
            root[F("state1good")] = F("Подключён");
            root[F("state1bad")] = F("");
        }
        root[F("elapsed")] = (uint32_t)(SETUP_TIME_SEC - millis() / 1000.0);
        root[F("factor_cold_feedback")] = factor1;
        root[F("factor_hot_feedback")] = get_auto_factor(runtime_data.impulses0, data.impulses0);
        root[F("error")] = F("");

    }
    else
    {
        root[F("factor_cold_feedback")] = 1;
        root[F("factor_hot_feedback")] = 1;
        root[F("error")] = F("Ошибка связи с МК");
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(root, *response);
    request->send(response);
};

/**
 * @brief Возвращает конфигурацию прибора в формате JSON для заполнения полей настройки
 *
 * @return
 */
void onGetConfig(AsyncWebServerRequest *request)
{
    if(captivePortal(request))
        return;
    LOG_INFO(F("Portal onGetConfig GET ") << request->host() << request->url());
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument json_doc(JSON_SMALL_STATIC_MSG_BUFFER);
    JsonObject root = json_doc.to<JsonObject>();
    root[FPSTR(PARAM_WMAIL)] = sett.waterius_email;
    root[FPSTR(PARAM_WHOST)] = sett.waterius_host;

    root[FPSTR(PARAM_BKEY)] = sett.blynk_key;
    root[FPSTR(PARAM_BHOST)] = sett.blynk_host;

    root[FPSTR(PARAM_MHOST)] = sett.mqtt_host;
    root[FPSTR(PARAM_MPORT)] = sett.mqtt_port;
    root[FPSTR(PARAM_MLOGIN)] = sett.mqtt_login;
    root[FPSTR(PARAM_MPASSWORD)] = sett.mqtt_password;
    root[FPSTR(PARAM_MTOPIC)] = sett.mqtt_topic;
    
    root[FPSTR(PARAM_SERIALCOLD)] = sett.serial0;
    root[FPSTR(PARAM_SERIALHOT)] = sett.serial1;

    root[FPSTR(PARAM_IP)] = IPAddress(sett.ip).toString();
    root[FPSTR(PARAM_GW)] = IPAddress(sett.gateway).toString();
    root[FPSTR(PARAM_SN)] = IPAddress(sett.mask).toString();

    root[FPSTR(PARAM_MPERIOD)] = sett.wakeup_per_min;

    root[FPSTR(PARAM_MDAUTO)] = sett.mqtt_auto_discovery;
    root[FPSTR(PARAM_MDTOPIC)] = sett.mqtt_discovery_topic;

    root[FPSTR(PARAM_NTP)] = sett.ntp_server;
    
    root[FPSTR(PARAM_S)] = WiFi.SSID().c_str();
    root[FPSTR(PARAM_P)] = WiFi.psk().c_str();
    root[FPSTR(PARAM_MAC)] = WiFi.macAddress();
    
    root[FPSTR(PARAM_CNAMEHOT)] = sett.counter0_name;
    root[FPSTR(PARAM_CNAMECOLD)] = sett.counter1_name;

    root[FPSTR(PARAM_CTYPEHOT)] = data.counter_type0;
    root[FPSTR(PARAM_CTYPECOLD)] = data.counter_type1;

    root[FPSTR(PARAM_FACTORHOT)] = sett.factor0;
    root[FPSTR(PARAM_FACTORCOLD)] = sett.factor1;

    root[FPSTR(PARAM_CH0)] = cdata.channel0;
    root[FPSTR(PARAM_CH1)] = cdata.channel1;
    
    LOG_INFO(F("JSON: Mem usage: ") << root.memoryUsage());
    LOG_INFO(F("JSON: Size: ") << measureJson(root));

    serializeJson(root, *response);
    request->send(response);
}


/**
 * @brief Обработчик POST запроса с новыми параметрами настройки прибора
 *
 * @return int
 */
void onPostWifiSave(AsyncWebServerRequest *request)
{
    LOG_INFO(F("Portal onPostWiFiSave POST ") << request->host()<< request->url());
    UpdateParamStr(request, FPSTR(PARAM_S), sett.wifi_ssid, WIFI_SSID_LEN - 1);
    UpdateParamStr(request, FPSTR(PARAM_P), sett.wifi_password, WIFI_PWD_LEN - 1);
    if (UpdateParamStr(request, FPSTR(PARAM_WMAIL), sett.waterius_email, EMAIL_LEN))
    {
        generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, sett.waterius_email);
    }
    SetParamStr(request, FPSTR(PARAM_WHOST), sett.waterius_host, HOST_LEN - 1);
    

#ifndef BLYNK_DISABLED
    SetParamStr(request, FPSTR(PARAM_BHOST), sett.blynk_host, HOST_LEN - 1);
    SetParamStr(request, FPSTR(PARAM_BKEY), sett.blynk_key, BLYNK_KEY_LEN);

#endif

// MQTT
#ifndef MQTT_DISABLED
    SetParamStr(request, FPSTR(PARAM_MHOST), sett.mqtt_host, HOST_LEN - 1);
    SetParamUInt(request, FPSTR(PARAM_MPORT), &sett.mqtt_port);
    SetParamStr(request, FPSTR(PARAM_MLOGIN), sett.mqtt_login, MQTT_LOGIN_LEN);
    SetParamStr(request, FPSTR(PARAM_MPASSWORD), sett.mqtt_password, MQTT_PASSWORD_LEN);
    SetParamStr(request, FPSTR(PARAM_MTOPIC), sett.mqtt_topic, MQTT_TOPIC_LEN);
    SetParamByte(request, FPSTR(PARAM_MDAUTO), &sett.mqtt_auto_discovery);
    SetParamStr(request, FPSTR(PARAM_MDTOPIC), sett.mqtt_discovery_topic, MQTT_TOPIC_LEN);

#endif // MQTT

    SetParamIP(request, FPSTR(PARAM_IP), &sett.ip);
    if (sett.ip)
    {
        SetParamIP(request, FPSTR(PARAM_GW), &sett.gateway);
        SetParamIP(request, FPSTR(PARAM_SN), &sett.mask);
        SetParamStr(request, FPSTR(PARAM_NTP), sett.ntp_server, HOST_LEN);
    }
    else
    {
        LOG_INFO(F("DHCP is ON"));
    }

    SetParamStr(request, FPSTR(PARAM_NTP), sett.ntp_server, HOST_LEN);
    LOG_INFO(F("NTP Server=") << sett.ntp_server);

    SetParamUInt(request, FPSTR(PARAM_MPERIOD), &sett.wakeup_per_min);
    sett.set_wakeup = sett.wakeup_per_min;
    LOG_INFO(F("wakeup period, min=") << sett.wakeup_per_min);
    LOG_INFO(F("wakeup period, tick=") << sett.set_wakeup);


    uint8_t counter0_type, counter1_type;
    SetParamByte(request, FPSTR(PARAM_CNAMEHOT), &sett.counter0_name);
    SetParamByte(request, FPSTR(PARAM_CNAMECOLD), &sett.counter1_name);
    SetParamByte(request, FPSTR(PARAM_CTYPEHOT), &counter0_type);
    SetParamByte(request, FPSTR(PARAM_CTYPECOLD), &counter1_type);
    
    if (!masterI2C.setCountersType(counter0_type, 
                                   counter1_type))
    {
        LOG_ERROR(F("Counters types wasn't set"));
    } //"Разбуди меня через..."
    else
    {
        LOG_INFO(F("Counter0 name: ") << sett.counter0_name << F(" type: ") << counter0_type);
        LOG_INFO(F("Counter1 name: ") << sett.counter1_name << F(" type: ") << counter1_type);
    }

    uint8_t combobox_factor = -1;
    if (SetParamByte(request, FPSTR(PARAM_FACTORCOLD), &combobox_factor))
    {
        sett.factor1 = get_factor(combobox_factor, data.impulses1, data.impulses1, 1);
        LOG_INFO("cold dropdown=" << combobox_factor);
        LOG_INFO("factorCold=" << sett.factor1);
    }
    if (SetParamByte(request, FPSTR(PARAM_FACTORHOT), &combobox_factor))
    {
        sett.factor0 = get_factor(combobox_factor, data.impulses0, data.impulses0, sett.factor1);
        LOG_INFO("hot dropdown=" << combobox_factor);
        LOG_INFO("factorHot=" << sett.factor0);
    }
    SetParamStr(request, FPSTR(PARAM_SERIALCOLD), sett.serial1, SERIAL_LEN);
    SetParamStr(request, FPSTR(PARAM_SERIALHOT), sett.serial0, SERIAL_LEN);

    if (SetParamFloat(request, FPSTR(PARAM_CH0), &sett.channel0_start))
    {
        sett.impulses0_start = data.impulses0;
        sett.impulses0_previous = sett.impulses0_start;
        LOG_INFO("impulses0=" << sett.impulses0_start);
    }
    if (SetParamFloat(request, FPSTR(PARAM_CH1), &sett.channel1_start))
    {
        sett.impulses1_start = data.impulses1;
        sett.impulses1_previous = sett.impulses1_start;
        LOG_INFO("impulses1=" << sett.impulses1_start);
    }

    sett.setup_time = millis();
    sett.setup_finished_counter++;

    store_config(sett);
    
    if (wifi_connect(sett)) {
        AsyncWebServerResponse *response = request->beginResponse(200, "", F("Save configuration - Successfully."));
        response->addHeader("Refresh", "2; url=/exit");
        request->send(response);
    }
    else 
    {
        request->redirect("/");
    }
}

void onErase(AsyncWebServerRequest *request)
{
    if (captivePortal(request))
        return;
    LOG_INFO(F("Portal onErase GET ") << request->host()<< request->url());
    ESP.eraseConfig();
    delay(100);
    LOG_INFO(F("EEPROM erased"));
    // The flash cache maps the physical flash into the address space at offset
    ESP.flashEraseSector(((EEPROM_start - 0x40200000) / SPI_FLASH_SEC_SIZE));
    LOG_INFO(F("0x40200000 erased"));
    delay(1000);
    ESP.reset();
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
    // запускаем сервер
    AsyncWebServer *server = new AsyncWebServer(80);
    server->on("/", HTTP_GET, onRoot);
    server->on("/fwlink", HTTP_GET, onRoot);
    server->on("/networks", HTTP_GET, onGetNetworks);
    server->on("/exit", HTTP_GET, onExit);
    server->onNotFound(onNotFound);
    server->serveStatic("/", LittleFS, "/");

    server->on("/states", HTTP_GET, onGetStates);
    server->on("/config", HTTP_GET, onGetConfig);
    server->on("/erase", HTTP_GET, onErase);
    server->on("/wifisave", HTTP_POST, onPostWifiSave);
    server->begin();

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
