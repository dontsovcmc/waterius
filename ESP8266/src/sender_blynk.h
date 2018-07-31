#ifndef _SENDERBLYNK_h
#define _SENDERBLYNK_h

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include "master_i2c.h"
#include "Logging.h"

#ifdef SEND_BLYNK
bool send_blynk(const Settings &sett, const float &value0, const float &value1, const float &voltage)
{
    bool ret = false;

    if (WiFi.status() != WL_CONNECTED) {
        LOG_NOTICE("BLK", "Wi-Fi not connected");
        return ret;
    }

    Blynk.config(sett.key, sett.hostname, BLYNK_DEFAULT_PORT);
    if (Blynk.connect(SERVER_TIMEOUT)) {
        
        LOG_NOTICE( "BLK", "run");

        unsigned int delta0 = (value0 - sett.prev_value0)*1000;  // litres
        unsigned int delta1 = (value1 - sett.prev_value1)*1000;

        Blynk.virtualWrite(V0, value0);
        Blynk.virtualWrite(V1, value1);
        Blynk.virtualWrite(V2, voltage);
        Blynk.virtualWrite(V3, delta0);
        Blynk.virtualWrite(V4, delta1);

        LOG_NOTICE( "BLK", "virtualWrite OK");

        if (sett.email) {
            LOG_NOTICE( "BLK", "send email");
            LOG_NOTICE( "BLK", "email: " << sett.email);

            String msg = sett.email_template;
            String title = sett.email_title;
            String v0(value0, 1);   //для образца СМС сообщения
            String v1(value1, 1);   //для образца СМС сообщения
            String v2(voltage, 3);
            String v3(delta0, 2);
            String v4(delta1, 2);
            
            msg.replace("{V0}", v0);
            msg.replace("{V1}", v1);
            msg.replace("{V2}", v2);
            msg.replace("{V3}", v3);
            msg.replace("{V4}", v4);
            
            title.replace("{V0}", v0);
            title.replace("{V1}", v1);
            title.replace("{V2}", v2);
            title.replace("{V3}", v3);
            title.replace("{V4}", v4);

            Blynk.email(sett.email, title, msg);

            LOG_NOTICE("BLK", "email was send");
            LOG_NOTICE("BLK", title);
            LOG_NOTICE("BLK", msg);
        }

        ret = true;
    } else {

        LOG_ERROR("BLK", "connect error");
    } 
    Blynk.disconnect();
    LOG_NOTICE("BLK", "disconnected");

    return ret;

    /*
    IPAddress myip = WiFi.localIP();
    BLYNK_LOG_IP("IP: ", myip);
    */
}		

#endif //#ifdef SEND_BLYNK

#endif
