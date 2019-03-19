#ifndef _SENDERJSON_h
#define _SENDERJSON_h

#include "setup.h"
#include "time.h"

#include "master_i2c.h"
#include "Logging.h"

#include "cert.h"
#include "utils.h"

#include <ArduinoJson.h>
#include "WifiClientSecure.h"
#include "ESP8266HTTPClient.h"

#ifdef SEND_WATERIUS

StaticJsonBuffer<1000> jsonBuffer;

BearSSL::X509List cert;
HTTPClient http;
WiFiClient client;
BearSSL::WiFiClientSecure client_tls;

void prepareJson(String& out, Settings &sett, const SlaveData &data, const float &channel0, const float &channel1) 
{
    JsonObject& root = jsonBuffer.createObject();
    root["key"] = sett.waterius_key;
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
    root.printTo(out);
}

bool send_json(Settings &sett, const SlaveData &data, const float &channel0, const float &channel1)
{
    String body;
    String url(sett.waterius_host);
    WiFiClient *c;

    LOG_NOTICE("JSN", "POST to: " << url);
    
    if (url.substring(0, 5) == "https") {
        c = &client_tls;

        cert.append(lets_encrypt_x3_ca);
        cert.append(lets_encrypt_x4_ca);
        cert.append(cloud_waterius_ru_ca);
        client_tls.setTrustAnchors(&cert);

        if (!setClock()) {  
            return false;
        }
        /*
        bool mfln = client.probeMaxFragmentLength("192.168.1.42", 5000, 1024);  // server must be the same as in ESPhttpUpdate.update()
        Serial.printf("MFLN supported: %s\n", mfln ? "yes" : "no");
        if (mfln) {
            client.setBufferSizes(1024, 1024);
        }*/
    } else {
        c = &client;
    }
    c->setTimeout(SERVER_TIMEOUT);

    prepareJson(body, sett, data, channel0, channel1);

    LOG_NOTICE("JSN", "json: " << body);

    http.setReuse(false); //only 1 request
    if (http.begin(*c, url)) {  // hostname); 
        http.addHeader("Content-Type", "application/json"); 

        int httpCode = http.POST(body);   //Send the request
        LOG_NOTICE("JSN", httpCode);

        body = http.getString();

        http.end();
        c->stop();

        if (httpCode == HTTP_CODE_OK) {
            LOG_NOTICE("JSN", "Responce: \n" << body);
            JsonObject& root = jsonBuffer.parseObject(body);
            if (root.success()) {
                LOG_NOTICE("JSN", body);
                return true;
            } else {
                LOG_ERROR("JSN", "parse response error");
            }
        } 
    }

    return false;
}

#endif  // SEND_HTTPS
#endif
