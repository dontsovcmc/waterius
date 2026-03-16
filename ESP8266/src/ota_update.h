#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "setup.h"
#include "voltage.h"

#define OTA_ERR_NONE 0
#define OTA_ERR_PARSE 1
#define OTA_ERR_FS_UPDATE 2
#define OTA_ERR_FW_UPDATE 3
#define OTA_ERR_LOW_BATTERY 4

#define OTA_MIN_VOLTAGE_MV 3300
#define OTA_USB_VOLTAGE_THRESHOLD_MV 4600

bool perform_ota_update(const JsonObject &ota, MasterI2C &masterI2C, Settings &sett, Voltage &voltage);

#endif
