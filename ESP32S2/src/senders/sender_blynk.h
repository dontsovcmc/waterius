#ifndef _SENDERBLYNK_h
#define _SENDERBLYNK_h

#ifndef BLYNK_DISABLED
// http://docs.blynk.cc/#widgets-notifications-email
// Максимальная длина тела электронного письма. М.б. до 1200
// email + subject + message length
#define BLYNK_MAX_SENDBYTES 512

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#endif
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "voltage.h"

bool send_blynk(const Settings &sett, DynamicJsonDocument &jsonData)
{
    if (!is_blynk(sett))
    {
        LOG_INFO(F("Blynk: SKIP"));
        return false;
    }

    Blynk.config(sett.blynk_key, sett.blynk_host, BLYNK_DEFAULT_PORT);
    if (Blynk.connect(SERVER_TIMEOUT))
    {

        LOG_INFO(F("Blynk: Run"));

        Blynk.virtualWrite(V0, jsonData[F("ch0")].as<float>());
        Blynk.virtualWrite(V1, jsonData[F("ch1")].as<float>());
        Blynk.virtualWrite(V2, jsonData[F("voltage")].as<float>());
        Blynk.virtualWrite(V3, jsonData[F("delta0")].as<int>());
        Blynk.virtualWrite(V4, jsonData[F("delta1")].as<int>());
        Blynk.virtualWrite(V5, jsonData[F("resets")].as<int>());
        Blynk.virtualWrite(V7, jsonData[F("voltage_diff")].as<float>());
        Blynk.virtualWrite(V8, jsonData[F("rssi")].as<signed int>());

        WidgetLED battery_led(V6);
        jsonData[F("voltage_low")].as<bool>() ? battery_led.on() : battery_led.off();

        LOG_INFO(F("virtualWrite OK"));

        Blynk.disconnect();
        LOG_INFO(F("Disconnected"));

        return true;
    }
    else
    {
        LOG_ERROR(F("Connect error"));
    }

    return false;
}
#endif
#endif