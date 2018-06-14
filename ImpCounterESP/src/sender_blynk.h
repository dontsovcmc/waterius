#ifndef _SENDERBLYNK_h
#define _SENDERBLYNK_h

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include "master_i2c.h"
#include "Logging.h"

bool send_blynk(const Settings &sett, const float &value0, const float &value1, const float &voltage)
{
    Blynk.begin(sett.key, WiFi.SSID().c_str(), WiFi.psk().c_str());
    LOG_NOTICE( "BLK", "Blynk beginned");

    if (Blynk.run())
    {
        LOG_NOTICE( "ESP", "Blynk run");

        Blynk.virtualWrite(V0, value0);
        Blynk.virtualWrite(V1, value1);
        Blynk.virtualWrite(V2, voltage);

        return true;
    } 
    LOG_ERROR("BLK", "Blynk connect error");
    return false;
}		

#endif