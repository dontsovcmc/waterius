
#include "send_data.h"
#include "json.h"
#include "senders/sender_waterius.h"
#include "senders/sender_http.h"
#include "senders/sender_mqtt.h"

void send_data(const Settings &sett, const AttinyData &data, const CalculatedData &cdata, JsonDocument &json_data)
{
    // Формироуем JSON
    get_json_data(sett, data, cdata, json_data);
    
    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

#ifndef WATERIUS_RU_DISABLED
    if (send_waterius(sett, json_data))
    {
        LOG_INFO(F("HTTP: Send OK"));
    }
#endif

#ifndef HTTPS_DISABLED
    if (send_http(sett, json_data))
    {
        LOG_INFO(F("HTTP: Send OK"));
    }
#endif

#ifndef MQTT_DISABLED
    if (is_mqtt(sett))
    {
        if (send_mqtt(sett, json_data))
        {
            LOG_INFO(F("MQTT: Send OK"));
        }
    }
    else
    {
        LOG_INFO(F("MQTT: SKIP"));
    }
#endif
}

bool settings_received(const JsonDocument &json_settings_received)
{
    return json_settings_received.size() > 0;
}