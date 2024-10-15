/**
 * @file sender_http.h
 * @brief Функции отправки сведений по htpp/https
 * @version 0.1
 * @date 2023-02-11
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef SENDERHTTP_h_
#define SENDERHTTP_h_
#ifndef HTTPS_DISABLED
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

#define HTTP_SEND_ATTEMPTS 3

bool send_http(const Settings &sett, DynamicJsonDocument &jsonData)
{
    if (!(sett.http_on && sett.http_url[0]))
    {
        LOG_INFO(F("HTTP: SKIP"));
        return false;
    };

    uint32_t start_time = millis();

    LOG_INFO(F("-- START -- "));
    LOG_INFO(F("HTTP: Send new data"));

    String payload = "";
    serializeJson(jsonData, payload);
    String url = sett.http_url;

    int attempts = HTTP_SEND_ATTEMPTS;
    bool result = false;
    do
    {
        LOG_INFO(F("HTTP: Attempt #") << HTTP_SEND_ATTEMPTS - attempts + 1 << F(" from ") << HTTP_SEND_ATTEMPTS);
        result = post_data(url, sett.waterius_key, sett.waterius_email, payload);

    } while (!result && --attempts);

    if (result)
    {
        LOG_INFO(F("HTTP: Data sent. Time ") << millis() - start_time << F(" ms"));
    }
    else
    {
        LOG_ERROR(F("HTTP: Failed send data. Time ") << millis() - start_time << F(" ms"));
    }

    LOG_INFO(F("-- END --"));

    return result;
}

#endif
#endif