/**
 * @file sender_waterius.h
 * @brief Функции отправки сведений в облако waterius.ru по htpp/https
 * @version 0.1
 * @date 2023-11-01
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef SENDERWATERIUS_h_
#define SENDERWATERIUS_h_
#ifndef WATERIUS_RU_DISABLED
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#endif
#include "setup.h"
#include "master_i2c.h"
#include "Logging.h"
#include "json.h"
#include "https_helpers.h"
#include "utils.h"


#define HTTP_SEND_ATTEMPTS 3

bool send_waterius(const Settings &sett, DynamicJsonDocument &jsonData)
{
    if (!is_waterius_site(sett))
    {
        LOG_INFO(F("WATR: SKIP"));
        return false;
    };

    uint32_t start_time = millis();

    LOG_INFO(F("-- START -- "));
    LOG_INFO(F("WATR: Send new data"));

    String payload = "";
    serializeJson(jsonData, payload);
    String url = sett.waterius_host;

    int attempts = HTTP_SEND_ATTEMPTS;
    bool result = false;
    do
    {
        LOG_INFO(F("WATR: Attempt #") << HTTP_SEND_ATTEMPTS - attempts + 1 << F(" from ") << HTTP_SEND_ATTEMPTS);
        result = post_data(url, sett.waterius_key, sett.waterius_email, payload);

    } while (!result && --attempts);

    if (result)
    {
        LOG_INFO(F("WATR: Data sent. Time ") << millis() - start_time << F(" ms"));
    }
    else
    {
        LOG_ERROR(F("WATR: Failed send data. Time ") << millis() - start_time << F(" ms"));
    }

    LOG_INFO(F("-- END --"));

    return result;
}

#endif
#endif