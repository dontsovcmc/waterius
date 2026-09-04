#include "send_data.h"
#include "json.h"
#include "senders/sender_waterius.h"
#include "senders/sender_http.h"
#include "senders/sender_mqtt.h"


void send_data(const Settings &sett, const AttinyData &data, const CalculatedData &cdata, JsonDocument &json_data, JsonDocument &json_settings, SessionStatus &status)
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
        if (send_mqtt(sett, json_data))
        {
            LOG_INFO(F("MQTT: Send OK"));
            status.mqtt = merge_status(status.mqtt, SEND_OK);
            status.delivered_any = true;
        }
        else
        {
            // Подключение к брокеру делает connect_and_subscribe_mqtt, сюда
            // приходим уже с готовым клиентом: не отправилось — значит связь
            status.mqtt = merge_status(status.mqtt, SEND_NO_CONNECTION);
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