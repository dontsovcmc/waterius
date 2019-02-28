#ifndef _SENDERHTTPS_h
#define _SENDERHTTPS_h

#include "setup.h"
#include "time.h"

#include "master_i2c.h"
#include "Logging.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "ESP8266httpUpdate.h"

#include <ArduinoJson.h>

#ifdef SEND_HTTPS

#include "cert.h"
#include "utils.h"

void prepareJson(JsonObject& root, Settings &sett, const SlaveData &data, const float &channel0, const float &channel1) 
{
    root["key"] = sett.key;
    root["version"] = data.version;
    root["version_esp"] = FIRMWARE_VERSION;
    root["boot"] = data.service;  // 2 - reset pin, 3 - power
    root["resets"] = data.resets;
    root["voltage"] = (float)(data.voltage / 1000.0);
    root["good"] = data.diagnostic;
    root["ch0"] = channel0;
    root["ch1"] = channel1;
    root["delta0"] = (channel0 - sett.channel0_previous)*1000;  // litres
    root["delta1"] = (channel1 - sett.channel1_previous)*1000;
    root["ca_crc"] = CRC16(0, (char*)digicert, strlen(digicert));
    root["ca2_crc"] = CRC16(0, sett.ca, strlen(sett.ca));
}


bool send_https(WiFiClient &client, const char *hostname, const int port, const char *path, String &request, String &responce) 
{
    HTTPClient http;
    http.setTimeout(SERVER_TIMEOUT);
    http.setReuse(true);
    
    if (http.begin(client, hostname, port, path)) {  // hostname); 
        http.addHeader("Content-Type", "application/json"); 

        int httpCode = http.POST(request);   //Send the request
        
        if (httpCode == HTTP_CODE_OK) {
            responce = http.getString();
            LOG_NOTICE("JSN", httpCode);
            http.end(); //Close connection
            return true;
        } 
        LOG_ERROR("JSN", httpCode);

        http.end();  //Close connection

    }
    return false;
}

#endif  // SEND_HTTPS
#endif
