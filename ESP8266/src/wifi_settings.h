#ifndef _WIFI_h
#define _WIFI_h

#include "setup.h"

#include <Arduino.h>
#include <WiFiClient.h>
#include "master_i2c.h"

#define FAKE_CRC 0422

/*
Сохраняем конфигурацию в EEPROM
*/
void storeConfig(const Settings &sett);

/*
Читаем конфигурацию из EEPROM
*/
bool loadConfig(Settings &sett);

#endif

