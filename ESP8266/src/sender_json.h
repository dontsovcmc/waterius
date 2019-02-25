#ifndef _SENDERJSON_h
#define _SENDERJSON_h

#include "setup.h"

#include "master_i2c.h"
#include "Logging.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#ifdef SEND_JSON

HTTPClient http;
WiFiClient client;

/*
Функция отправляющая данные в JSON на TCP сервер.
URL HTTP сервера: sett.hostname_json
*/
bool send_json(const Settings &sett, const SlaveData &data, const float &channel0, const float &channel1)
{
    bool connect = false;

    if (strnlen(sett.hostname_json, HOSTNAME_JSON_LEN))
    {
        LOG_NOTICE("JSN", "Making HTTP connection to: " << sett.hostname_json);

        http.setTimeout(SERVER_TIMEOUT);
        connect = http.begin(client, sett.hostname_json);      //Specify request destination
        
        if (connect) {

            http.addHeader("Content-Type", "application/json"); 
            http.addHeader("Connection", "close");

            StaticJsonBuffer<300> jsonBuffer; 
            JsonObject& root = jsonBuffer.createObject();
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

            String output;
            root.printTo(output);
            LOG_NOTICE("JSN", output);

            int httpCode = http.POST(output);   //Send the request
            String payload = http.getString();  //Get the response payload

            if (httpCode == 200) {
                LOG_NOTICE("JSN", httpCode);
            } else {
                LOG_ERROR("JSN", httpCode);
            }
            
            LOG_NOTICE("JSN", payload);
            http.end();  //Close connection
            
        } else {
            LOG_ERROR("JSN", "Connection to server failed");
        }
    }
      
    return false;
}

#endif  // SEND_JSON
#endif
