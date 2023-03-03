#ifndef _SENDERBLYNK_h
#define _SENDERBLYNK_h

#ifndef BLYNK_DISABLED
// http://docs.blynk.cc/#widgets-notifications-email
// Максимальная длина тела электронного письма. М.б. до 1200
// email + subject + message length
#define BLYNK_MAX_SENDBYTES 512

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "voltage.h"


bool send_blynk(const Settings &sett, JsonDocument &jsonData)
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

        // Если заполнен параметр email отправим эл. письмо
        if (strnlen(sett.blynk_email, EMAIL_LEN) > 4)
        {
            LOG_INFO("send email: " << sett.blynk_email);

            String msg = sett.blynk_email_template;
            String title = sett.blynk_email_title;
            String v0(jsonData[F("ch0")].as<float>(), 1); //.1 для образца СМС сообщения
            String v1(jsonData[F("ch1")].as<float>(), 1); //.1 для образца СМС сообщения
            String v2(jsonData[F("voltage")].as<float>(), 3);
            String v3(jsonData[F("delta0")].as<unsigned int>(), DEC);
            String v4(jsonData[F("delta1")].as<unsigned int>(), DEC);
            String v5(jsonData[F("resets")].as<unsigned int>(), DEC);
            String v6(jsonData[F("voltage_low")].as<bool>(),DEC);
            String v7(jsonData[F("voltage_diff")].as<float>(), 3);
            String v8(jsonData[F("rssi")].as<signed int>(), DEC);

            msg.replace(F("{V0}"), v0);
            msg.replace(F("{V1}"), v1);
            msg.replace(F("{V2}"), v2);
            msg.replace(F("{V3}"), v3);
            msg.replace(F("{V4}"), v4);
            msg.replace(F("{V5}"), v5);
            msg.replace(F("{V6}"), v6);
            msg.replace(F("{V7}"), v7);
            msg.replace(F("{V8}"), v8);

            title.replace(F("{V0}"), v0);
            title.replace(F("{V1}"), v1);
            title.replace(F("{V2}"), v2);
            title.replace(F("{V3}"), v3);
            title.replace(F("{V4}"), v4);
            title.replace(F("{V5}"), v5);
            title.replace(F("{V6}"), v6);
            title.replace(F("{V7}"), v7);
            title.replace(F("{V8}"), v8);

            Blynk.email(sett.blynk_email, title, msg);

            LOG_INFO(F("Email was sent"));
            LOG_INFO(title);
            LOG_INFO(msg);
        }

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