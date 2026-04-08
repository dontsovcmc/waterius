#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include "ota_parse.h"
#include "master_i2c.h"
#include "setup.h"
#include "voltage.h"

#define OTA_MIN_VOLTAGE_MV 3300
#define OTA_USB_VOLTAGE_THRESHOLD_MV 4600

bool perform_ota_update(const JsonObject &ota, MasterI2C &masterI2C, Settings &sett, Voltage &voltage);

#endif
