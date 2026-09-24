#include "send_data.h"
#include "json.h"
#include "senders/sender_waterius.h"
#include "senders/sender_http.h"
#include "senders/sender_mqtt.h"


void send_data(Settings &sett, const AttinyData &data, const CalculatedData &cdata, JsonDocument &json_data, JsonDocument &json_settings, SessionStatus &status)
{
    // Формироуем JSON
    get_json_data(sett, data, cdata, json_data);


    LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

#ifndef WATERIUS_RU_DISABLED
    SendStatus waterius_status = send_waterius(sett, json_data, json_settings);
    if (waterius_status == SEND_OK)
    {
        LOG_INFO(F("HTTP: Send OK"));
        status.delivered_any = true;
    }
    status.waterius = merge_status(status.waterius, waterius_status);
#endif

#ifndef HTTPS_DISABLED
    SendStatus http_status = send_http(sett, json_data, json_settings);
    if (http_status == SEND_OK)
    {
        LOG_INFO(F("HTTP: Send OK"));
        status.delivered_any = true;
    }
    status.http = merge_status(status.http, http_status);
#endif

#ifndef MQTT_DISABLED
    if (is_mqtt(sett))
    {
        SendStatus mqtt_status = send_mqtt(sett, data, json_data);
        if (mqtt_status == SEND_OK)
        {
            LOG_INFO(F("MQTT: Send OK"));
            status.delivered_any = true;
        }
        status.mqtt = merge_status(status.mqtt, mqtt_status);
    }
    else
    {
        LOG_INFO(F("MQTT: SKIP"));
    }
#endif
}

bool settings_received(const JsonDocument &json_settings_received)
{
    if (json_settings_received.size() == 0)
    {
        return false;
    }
    // Только OTA — не считаем за настройки
    if (json_settings_received.size() == 1 && has_ota(json_settings_received))
    {
        return false;
    }
    return true;
}