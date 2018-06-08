#ifndef _WIFI_h
#define _WIFI_h

#include "setup.h"

#include <Arduino.h>
#include <WiFiClient.h>
#include "MasterI2C.h"


void storeConfig(const Settings &sett);
bool loadConfig(Settings &sett);


#endif

