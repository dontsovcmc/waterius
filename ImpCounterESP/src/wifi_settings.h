#ifndef _WIFI_h
#define _WIFI_h

#include "setup.h"

#include <Arduino.h>
#include <WiFiClient.h>
#include "master_i2c.h"


void storeConfig(const Settings &sett);
bool loadConfig(Settings &sett);


#endif

