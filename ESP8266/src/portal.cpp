#include "portal.h"
#include "Logging.h"
#include "master_i2c.h"
#include "utils.h"
#include "config.h"
#include "ESPAsyncWebServer.h"
#include "WebHandlerImpl.h"
#include "json_constructor.h"

extern SlaveData data;
extern MasterI2C masterI2C;
extern Settings sett;
extern CalculatedData cdata;

#define SETUP_TIME_SEC 600UL

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

bool Portal::isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9') && c !='/') {
      return false;
    }
  }
  return true;
}

String Portal::ipToString(uint32_t ip)
{
    String ret="";
    ret.reserve(16);
    uint8_t *src=(uint8_t*)&ip;
    uint8_t i=3;
    while(i--)
    {
        ret += String(*src++);
        ret +=F(".");
    }
    ret += String(*src++);
    return ret;
}

bool Portal::captivePortal(AsyncWebServerRequest *request)
{
    if(isIp(request->host()))
        return false;
    String url=String("http://") + ipToString(request->client()->getLocalAddress());
    LOG_INFO(F("Request redirected to captive portal ")<< url);
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", url);
    request->send(response);
    return true;
}

void Portal::onGetRoot(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer onGetRoot GET ") << request->host() << request->url());
    if (captivePortal(request))
        return;
    request->send(LittleFS, "/index.html");
}

void Portal::onGetScript(AsyncWebServerRequest *request)
{
    if (captivePortal(request))
        return;
    LOG_INFO(F("AsyncWebServer onGetScript GET ") << request->host() << request->url());
    request->send(LittleFS, "/script.js");
};

void Portal::onGetNetworks(AsyncWebServerRequest *request)
{
    if(captivePortal(request))
        return;
    LOG_INFO(F("AsyncWebServer onGetNetworks GET ") << request->host() << request->url());
    int n = WiFi.scanComplete();
    if (n == -2)
    {
        WiFi.scanNetworks(true);
        request->send(100, "", "");
    }
    else if (n)
    {
        String json = "";
        for (int i = 0; i < n; ++i)
        {
            LOG_INFO(WiFi.SSID(i) << " " << WiFi.RSSI(i));
            json += F("<label class='radcnt' onclick='c(this)'>");
            json += String(WiFi.SSID(i));
            json += F("<input type='radio' name='n'><span class='rmrk'></span><div role='img' class='q q-");
            json += String(int(round(map(WiFi.RSSI(i), -100, -50, 1, 4))));
            if (WiFi.encryptionType(WiFi.encryptionType(i)) != ENC_TYPE_NONE)
            {
                json += F(" l");
            }
            json += F("'></div></label>");
        }
        WiFi.scanDelete();
        if (WiFi.scanComplete() == -2)
        {
            WiFi.scanNetworks(true);
        }
        json += F("<br/>");
        request->send(200, "", json);
        json = String();
    }
};

/**
 * @brief Возвращает конфигурацию прибора в формате JSON для заполнения полей настройки
 *
 * @return
 */
void Portal::onGetConfig(AsyncWebServerRequest *request)
{
    if(captivePortal(request))
        return;
    LOG_INFO(F("AsyncWebServer onGetConfig GET ") << request->host() << request->url());
    JsonConstructor json(2048);
    json.begin();
    json.push(F("wmail"), sett.waterius_email);
    json.push(F("whost"), sett.waterius_host);
    json.push(F("mperiod"), sett.wakeup_per_min);
    json.push(F("s"), WiFi.SSID().c_str());
    json.push(F("p"), WiFi.psk().c_str());
    json.push(F("bhost"), sett.blynk_host);
    json.push(F("bkey"), sett.blynk_key);
    json.push(F("bemail"), sett.blynk_email);
    json.push(F("btitle"), sett.blynk_email_title);
    json.push(F("btemplate"), sett.blynk_email_template);
    json.push(F("mhost"), sett.mqtt_host);
    json.push(F("mport"), sett.mqtt_port);
    json.push(F("mlogin"), sett.mqtt_login);
    json.push(F("mpassword"), sett.mqtt_password);
    json.push(F("mtopic"), sett.mqtt_topic);
    json.push(F("auto_discovery_checkbox"), sett.mqtt_auto_discovery);
    json.push(F("discovery_topic"), sett.mqtt_discovery_topic);
    json.push(F("mac"), WiFi.macAddress().c_str());
    json.push(F("ip"), ipToString(sett.ip).c_str());
    json.push(F("sn"), ipToString(sett.mask).c_str());
    json.push(F("gw"), ipToString(sett.gateway).c_str());
    json.push(F("ntp"), sett.ntp_server);
    json.push(F("factorCold"), sett.factor1);
    json.push(F("factorHot"), sett.factor0);
    json.push(F("serialCold"), sett.serial0);
    json.push(F("serialHot"), sett.serial1);
    json.push(F("ch0"), cdata.channel0, 3);
    json.push(F("ch1"), cdata.channel1, 3);
    json.end();
    AsyncResponseStream *response = request->beginResponseStream(F("application/json"));
    response->addHeader("Server", "ESP Async Web Server");
    response->print(json.c_str());
    request->send(response);
};

/**
 * @brief Возвращает параметры считанные с Waterius`a для отображения данных в реальном времени
 *
 * @return int
 */
void Portal::onGetStates(AsyncWebServerRequest *request)
{
    if(captivePortal(request))
        return;
    LOG_INFO(F("AsyncWebServer onGetState GET ") << request->host() << request->url());
    SlaveData runtime_data;
    JsonConstructor json(1024);
    json.begin();
    if (masterI2C.getSlaveData(runtime_data))
    {
        if (runtime_data.impulses0 > data.impulses0)
        {
            json.push(F("state0good"), F("Подключён"));
            json.push(F("state0bad"), F(""));
        }
        else
        {
            json.push(F("state0good"), F(""));
            json.push(F("state0bad"), F("Подключён"));
        }
        if (runtime_data.impulses1 > data.impulses1)
        {
            json.push(F("state1good"), F("Подключён"));
            json.push(F("state1bad"), F(""));
        }
        else
        {
            json.push(F("state1good"), F(""));
            json.push(F("state1bad"), F("Подключён"));
        }
        json.push(F("elapsed"), (uint32_t)(SETUP_TIME_SEC - millis() / 1000.0));
        json.push(F("factor_cold_feedback"), get_auto_factor(runtime_data.impulses1, data.impulses1));
        json.push(F("factor_hot_feedback"), get_auto_factor(runtime_data.impulses0, data.impulses0));
        if (_fail)
        {
            json.push(F("fail"), F("1"));
        }
        else
        {
            json.push(F("fail"), F(""));
        }
        json.push(F("error"), F(""));
    }
    else
    {
        json.push(F("error"), F("Ошибка связи с МК"));
        json.push(F("factor_cold_feedback"), 1);
        json.push(F("factor_hot_feedback"), 1);
    }
    json.end();
    request->send(200, F("application/json"), json.c_str());
};

bool Portal::UpdateParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size)
{
    if (request->hasParam(param_name, true))
    {
        if (strcmp(dest, request->getParam(param_name, true)->value().c_str()) != 0)
        {
            return SetParamStr(request, param_name, dest, size);
        }
        else
        {
            LOG_INFO(F("No modify ") << param_name << F("=") << dest);
            return false;
        }
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size)
{
    if (request->hasParam(param_name, true))
    {
        strncpy0(dest, request->getParam(param_name, true)->value().c_str(), size);
        LOG_INFO(F("Save ") << param_name << F("=") << dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamIP(AsyncWebServerRequest *request, const char *param_name, uint32_t *dest)
{
    if (request->hasParam(param_name, true))
    {
        IPAddress ip;
        if (ip.fromString(request->getParam(param_name, true)->value()))
        {
            *dest = ip.v4();
            LOG_INFO(F("Save ") << param_name << F("=") << ip.toString());
            return true;
        }
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamUInt(AsyncWebServerRequest *request, const char *param_name, uint16_t *dest)
{
    if (request->hasParam(param_name, true))
    {

        *dest = (uint16_t)(request->getParam(param_name, true)->value().toInt());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamByte(AsyncWebServerRequest *request, const char *param_name, uint8_t *dest)
{
    if (request->hasParam(param_name, true))
    {
        *dest = (uint8_t)(request->getParam(param_name, true)->value().toInt());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamFloat(AsyncWebServerRequest *request, const char *param_name, float *dest)
{
    if (request->hasParam(param_name, true))
    {
        *dest = (uint8_t)(request->getParam(param_name, true)->value().toFloat());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

/**
 * @brief Обработчик POST запроса с новыми параметрами настройки прибора
 *
 * @return int
 */
void Portal::onPostWifiSave(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer onPostWiFiSave POST ") << request->host()<< request->url());
    _fail = false;
    UpdateParamStr(request, "s", sett.wifi_ssid, WIFI_SSID_LEN - 1);
    UpdateParamStr(request, "p", sett.wifi_password, WIFI_PWD_LEN - 1);
    if (UpdateParamStr(request, PARAM_WMAIL, sett.waterius_email, EMAIL_LEN))
    {
        generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, sett.waterius_email);
    }
    SetParamStr(request, PARAM_WHOST, sett.waterius_host, HOST_LEN - 1);
    SetParamUInt(request, PARAM_MPERIOD, &sett.wakeup_per_min);

    SetParamStr(request, PARAM_BHOST, sett.blynk_host, HOST_LEN - 1);
    SetParamStr(request, PARAM_BKEY, sett.blynk_key, BLYNK_KEY_LEN);
    SetParamStr(request, PARAM_BMAIL, sett.blynk_email, EMAIL_LEN);
    SetParamStr(request, PARAM_BTITLE, sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN);
    SetParamStr(request, PARAM_BTEMPLATE, sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN);

    SetParamStr(request, PARAM_MHOST, sett.mqtt_host, HOST_LEN - 1);
    SetParamUInt(request, PARAM_MPORT, &sett.mqtt_port);
    SetParamStr(request, PARAM_MLOGIN, sett.mqtt_login, MQTT_LOGIN_LEN);
    SetParamStr(request, PARAM_MPASSWORD, sett.mqtt_password, MQTT_PASSWORD_LEN);
    SetParamStr(request, PARAM_MTOPIC, sett.mqtt_topic, MQTT_TOPIC_LEN);
    SetParamByte(request, PARAM_MDAUTO, &sett.mqtt_auto_discovery);
    SetParamStr(request, PARAM_MDTOPIC, sett.mqtt_discovery_topic, MQTT_TOPIC_LEN);

    SetParamIP(request, PARAM_IP, &sett.ip);
    SetParamIP(request, PARAM_GW, &sett.gateway);
    SetParamIP(request, PARAM_SN, &sett.mask);
    SetParamStr(request, PARAM_NTP, sett.ntp_server, HOST_LEN);

    uint8_t combobox_factor = -1;
    if (SetParamByte(request, PARAM_FACTORCOLD, &combobox_factor))
    {
        sett.factor1 = get_factor(combobox_factor, data.impulses1, data.impulses1, 1);
        LOG_INFO("cold dropdown=" << combobox_factor);
        LOG_INFO("factorCold=" << sett.factor1);
    }
    if (SetParamByte(request, PARAM_FACTORHOT, &combobox_factor))
    {
        sett.factor0 = get_factor(combobox_factor, data.impulses1, data.impulses1, 1);
        LOG_INFO("hot dropdown=" << combobox_factor);
        LOG_INFO("factorHot=" << sett.factor0);
    }
    SetParamStr(request, PARAM_SERIALCOLD, sett.serial1, SERIAL_LEN);
    SetParamStr(request, PARAM_SERIALHOT, sett.serial0, SERIAL_LEN);

    if (SetParamFloat(request, PARAM_CH0, &sett.channel0_start))
    {
        sett.impulses0_start = data.impulses0;
        sett.impulses0_previous = sett.impulses0_start;
        LOG_INFO("impulses0=" << sett.impulses0_start);
    }
    if (SetParamFloat(request, PARAM_CH1, &sett.channel1_start))
    {
        sett.impulses1_start = data.impulses1;
        sett.impulses1_previous = sett.impulses1_start;
        LOG_INFO("impulses1=" << sett.impulses1_start);
    }

    WiFi.persistent(true);
    _fail = not WiFi.begin(sett.wifi_ssid, sett.wifi_password);
    WiFi.persistent(false);
    // Запоминаем кол-во импульсов Attiny соответствующих текущим показаниям счетчиков
    if (!_fail)
    {
        sett.setup_time = millis();
        sett.setup_finished_counter++;

        store_config(sett);
        _delaydonesettings = millis() + 10000;
        _donesettings = true;
        AsyncWebServerResponse *response = request->beginResponse(200, "", F("Save configuration - Successfully."));
        response->addHeader("Refresh", "2; url=/exit");
        request->send(response);
    }
    else
    {
        // request->send(LittleFS, "/fail.html");
        request->redirect("/");
    }
};

void Portal::onNotFound(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer 404 ") << request->host()<< request->url());
    if(captivePortal(request))
        return;
    request->send(404);
};

void Portal::onExit(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer onExit GET ") << request->host()<< request->url());
    request->redirect("/");
    _donesettings = true;
    _delaydonesettings = millis();
};

void Portal::onErase(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer onErase GET ") << request->host()<< request->url());
    request->redirect("/");
    code = 2;
    _donesettings = true;
    _donesettings = millis();
};

bool Portal::doneettings()
{
    return _donesettings && _delaydonesettings < millis();
}

void Portal::begin()
{
    _donesettings = false;
    _delaydonesettings = millis();
    _fail = false;
    code = 0;
    server->begin();
}

void Portal::end()
{
    server->end();
}

AsyncCallbackWebHandler& Portal::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest)
{
    return server->on(uri, method, onRequest);
}

Portal::Portal()
{
    _donesettings = false;
    _fail = false;
    /* web server*/
    server = new AsyncWebServer(80);
    server->on("/", HTTP_GET, std::bind(&Portal::onGetRoot, this, std::placeholders::_1));
    server->on("/fwlink", HTTP_GET, std::bind(&Portal::onGetRoot, this, std::placeholders::_1));
    server->on("/script.js", HTTP_GET, std::bind(&Portal::onGetScript, this, std::placeholders::_1));
    server->on("/networks", HTTP_GET, std::bind(&Portal::onGetNetworks, this, std::placeholders::_1));
    server->on("/wifisave", HTTP_POST, std::bind(&Portal::onPostWifiSave, this, std::placeholders::_1));
    server->on("/config", HTTP_GET, std::bind(&Portal::onGetConfig, this, std::placeholders::_1));
    server->on("/exit", HTTP_GET, std::bind(&Portal::onExit, this, std::placeholders::_1));
    server->on("/erase", HTTP_GET, std::bind(&Portal::onErase, this, std::placeholders::_1));
    server->onNotFound(std::bind(&Portal::onNotFound, this, std::placeholders::_1));
}

Portal::~Portal()
{
    delete server;
}