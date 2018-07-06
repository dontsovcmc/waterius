#ifndef _WIFI_h
#define _WIFI_h

#include "setup.h"

#include <Arduino.h>
#include <WiFiClient.h>
#include "master_i2c.h"

#define FAKE_CRC 1237

void storeConfig(const Settings &sett);
bool loadConfig(Settings &sett);


#endif

