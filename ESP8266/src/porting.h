#ifndef waterius_porting_h
#define waterius_porting_h

#ifdef ESP8266
    extern "C" {
      #include "user_interface.h"
    }
    #include <ESP8266WiFi.h>

    #define getChipId() ESP.getChipId()
    
#elif defined(ESP32)
    #include <WiFi.h>
    #include <esp_wifi.h>  

    #define getChipId() (uint32_t)ESP.getEfuseMac()
#endif

#endif