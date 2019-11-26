#ifndef _SENDERBLYNK_h
#define _SENDERBLYNK_h

// http://docs.blynk.cc/#widgets-notifications-email
// Максимальная длина тела электронного письма. М.б. до 1200
// email + subject + message length
#define BLYNK_MAX_SENDBYTES 512


#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include "master_i2c.h"
#include "Logging.h"

bool send_blynk(const Settings &sett, const SlaveData &data, const CalculatedData &cdata)
{
    if (strnlen(sett.blynk_key, BLYNK_KEY_LEN) == 0) {
        LOG_NOTICE(FPSTR(S_BLK), FPSTR(S_SKIP));
        return false;
    }
        
    Blynk.config(sett.blynk_key, sett.blynk_host, BLYNK_DEFAULT_PORT);
    if (Blynk.connect(SERVER_TIMEOUT)) {
        
        LOG_NOTICE(FPSTR(S_BLK), FPSTR(S_RUN));

        // LOG_INFO(FPSTR(S_BLK), "Delta0:" << cdata.delta0);
        // LOG_INFO(FPSTR(S_BLK), "Delta1:" << cdata.delta1);

        Blynk.virtualWrite(V0, cdata.channel0);
        Blynk.virtualWrite(V1, cdata.channel1);
        Blynk.virtualWrite(V2, (float)(data.voltage / 1000.0));
        Blynk.virtualWrite(V3, cdata.delta0);
        Blynk.virtualWrite(V4, cdata.delta1);
        Blynk.virtualWrite(V5, data.resets);
        Blynk.virtualWrite(V7, (float)(cdata.voltage_diff / 1000.0));
        Blynk.virtualWrite(V8, cdata.rssi);

        WidgetLED battery_led(V6);
        cdata.low_voltage ? battery_led.on() : battery_led.off();

        LOG_NOTICE(FPSTR(S_BLK), FPSTR(S_VIRT_OK));
        
        // Если заполнен параметр email отправим эл. письмо
        if (strnlen(sett.blynk_email, EMAIL_LEN) > 4) {
            LOG_NOTICE(FPSTR(S_BLK), "send email: " << sett.blynk_email);

            String msg = sett.blynk_email_template;
            String title = sett.blynk_email_title;
            String v0(cdata.channel0, 1);   //.1 для образца СМС сообщения
            String v1(cdata.channel1, 1);   //.1 для образца СМС сообщения
            String v2((float)(data.voltage / 1000.0), 3);
            String v3(cdata.delta0, DEC);
            String v4(cdata.delta1, DEC);
            String v5(data.resets, DEC);
            String v6(cdata.low_voltage, DEC);
            String v7((float)(cdata.voltage_diff / 1000.0), 3);
            String v8(cdata.rssi, DEC);
            
            msg.replace(FPSTR(S_V0), v0);
            msg.replace(FPSTR(S_V1), v1);
            msg.replace(FPSTR(S_V2), v2);
            msg.replace(FPSTR(S_V3), v3);
            msg.replace(FPSTR(S_V4), v4);
            msg.replace(FPSTR(S_V5), v5);
            msg.replace(FPSTR(S_V6), v6);
            msg.replace(FPSTR(S_V7), v7);
            msg.replace(FPSTR(S_V8), v8);
            
            title.replace(FPSTR(S_V0), v0);
            title.replace(FPSTR(S_V1), v1);
            title.replace(FPSTR(S_V2), v2);
            title.replace(FPSTR(S_V3), v3);
            title.replace(FPSTR(S_V4), v4);
            title.replace(FPSTR(S_V5), v5);
            title.replace(FPSTR(S_V6), v6);
            title.replace(FPSTR(S_V7), v7);
            title.replace(FPSTR(S_V8), v8);

            Blynk.email(sett.blynk_email, title, msg);

            LOG_NOTICE(FPSTR(S_BLK), FPSTR(S_EMAIL_SEND));
            LOG_NOTICE(FPSTR(S_BLK), title);
            LOG_NOTICE(FPSTR(S_BLK), msg);
        }

        Blynk.disconnect();
        LOG_NOTICE(FPSTR(S_BLK), FPSTR(S_DISCONNECTED));

        return true;
    } else {
        LOG_ERROR(FPSTR(S_BLK), FPSTR(S_CONNECT_ERROR));
    } 

    return false;
}        

#endif
