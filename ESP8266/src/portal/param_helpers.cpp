#include "param_helpers.h"
#include "Logging.h"
#include "master_i2c.h"
#include "utils.h"
#include "config.h"
#include "ESPAsyncWebServer.h"

#define SETUP_TIME_SEC 600UL


bool SetParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size)
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


bool UpdateParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size)
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

bool SetParamIP(AsyncWebServerRequest *request, const char *param_name, uint32_t *dest)
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

bool SetParamUInt(AsyncWebServerRequest *request, const char *param_name, uint16_t *dest)
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

bool SetParamByte(AsyncWebServerRequest *request, const char *param_name, uint8_t *dest)
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

bool SetParamFloat(AsyncWebServerRequest *request, const char *param_name, float *dest)
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







bool UpdateParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size)
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

bool SetParamStr(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, char *dest, size_t size)
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

bool SetParamIP(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint32_t *dest)
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

bool SetParamUInt(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint16_t *dest)
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

bool SetParamByte(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, uint8_t *dest)
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

bool SetParamFloat(AsyncWebServerRequest *request, const __FlashStringHelper *param_name, float *dest)
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
