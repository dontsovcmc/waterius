#ifndef HA_APPLY_SETTINGS_H_
#define HA_APPLY_SETTINGS_H_

#include <ArduinoJson.h>
#include "setup.h"
#include "master_i2c.h"

void apply_settings(const JsonDocument &json_settings_received, Settings &sett, const AttinyData &data, CalculatedData &cdata);

#endif
