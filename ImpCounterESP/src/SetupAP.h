#ifndef _SETUPAP_h
#define _SETUPAP_h

#include "Setup.h"

#include <Arduino.h>
#include <WiFiClient.h>

void setup_ap(Settings &sett, const SlaveData &data);
void storeConfig(const Settings &sett);
bool loadConfig(Settings &sett);

#endif

