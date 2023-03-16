#include "portal.h"
#include "Logging.h"
#include "master_i2c.h"
#include "utils.h"
#include "config.h"
#include "ESPAsyncWebServer.h"
#include "WebHandlerImpl.h"
#include "json_constructor.h"

#define SETUP_TIME_SEC 600UL

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
    LOG_INFO(F("Fail ") << param_name);
    return false;
}







bool Portal::UpdateParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size)
{
    if (request->hasParam(param_name, true))
    {
        const char* new_value=request->getParam(FPSTR(param_name), true)->value().c_str();
        if (strcmp(dest, new_value) != 0)
        {
            return SetParamStr(request, param_name, dest, size);
        }
        else
        {
            LOG_INFO(F("No modify ") << param_name << F("=") << dest);
            return false;
        }
    }
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size)
{
    if (request->hasParam(param_name, true))
    {
        strncpy0(dest, request->getParam(param_name, true)->value().c_str(), size);
        LOG_INFO(F("Save ") << param_name << F("=") << dest);
        return true;
    }
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamIP(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint32_t *dest)
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
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamUInt(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint16_t *dest)
{
    if (request->hasParam(param_name, true))
    {

        *dest = (uint16_t)(request->getParam(param_name, true)->value().toInt());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamByte(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint8_t *dest)
{
    if (request->hasParam(param_name, true))
    {
        *dest = (uint8_t)(request->getParam(param_name, true)->value().toInt());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamFloat(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, float *dest)
{
    if (request->hasParam(param_name, true))
    {
        *dest = (uint8_t)(request->getParam(param_name, true)->value().toFloat());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

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
    _delaydonesettings = millis()+1000;
};

bool Portal::doneettings()
{
    return _donesettings && _delaydonesettings < millis();
}

void Portal::begin()
{
    _donesettings = false;
    _delaydonesettings = millis();
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
    /* web server*/
    server = new AsyncWebServer(80);
    server->on("/", HTTP_GET, std::bind(&Portal::onGetRoot, this, std::placeholders::_1));
    server->on("/fwlink", HTTP_GET, std::bind(&Portal::onGetRoot, this, std::placeholders::_1));
    server->on("/networks", HTTP_GET, std::bind(&Portal::onGetNetworks, this, std::placeholders::_1));
    server->on("/exit", HTTP_GET, std::bind(&Portal::onExit, this, std::placeholders::_1));
    server->onNotFound(std::bind(&Portal::onNotFound, this, std::placeholders::_1));
    server->serveStatic("/", LittleFS, "/");
}

Portal::~Portal()
{
    delete server;
}