#ifndef _SENDERBLYNK_h
#define _SENDERBLYNK_h

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

//#include "WifiSettings.h"
#include "MasterI2C.h"
#include "Logging.h"

bool send_blynk(const Settings &sett, const float &value0, const float &value1, const float &voltage)
{
    Blynk.begin(sett.key, WiFi.SSID().c_str(), WiFi.psk().c_str());
    LOG_NOTICE( "ESP", "Blynk beginned");

    if (Blynk.run())
    {
        LOG_NOTICE( "ESP", "Blynk run");

        Blynk.virtualWrite(V0, value0);
        Blynk.virtualWrite(V1, value1);
        Blynk.virtualWrite(V2, voltage);

        return true;
    }
    return false;
}		

#endif