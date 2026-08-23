#ifndef SEND_DATA_H_
#define SEND_DATA_H_

#include "setup.h"
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "core/blink.h"

/*
Итог отправки складывается в status: облачные получатели в status.cloud,
MQTT в status.mqtt. Отсюда светодиод узнаёт, чем закончился сеанс.
*/
void send_data(const Settings &sett, const AttinyData &data, const CalculatedData &cdata, JsonDocument &json_data, JsonDocument &json_settings, SessionStatus &status);
bool settings_received(const JsonDocument &json_settings_received);

inline bool has_ota(const JsonDocument &json_settings_received)
{
    return json_settings_received.containsKey(F("ota"));
}

#ifndef MQTT_DISABLED
bool connect_and_subscribe_mqtt(Settings &sett, JsonDocument &json_settings_received);
#endif

#endif
