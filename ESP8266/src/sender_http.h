/**
 * @file sender_http.h
 * @brief Функции отправки сведений по htpp/https
 * @version 0.1
 * @date 2023-01-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef _SENDERHTTP_h
#define _SENDERHTTP_h
# ifndef HTTPS_DISABLED
#include <ESP8266WiFi.h>
#include "setup.h"
#include "master_i2c.h"
#include "WateriusHttps.h"
#include "Logging.h"
#include "json.h"

bool send_http(const String &url, const Settings &sett, DynamicJsonDocument &jsonData)
{
    LOG_INFO(F("HTTP: -- START -- ") << F("Send new data"));

    if(!(sett.waterius_host[0] && sett.waterius_key[0]))
    {
        LOG_INFO(F("HTTP: SKIP"));
        return false;
    };

    String payload = "";
    serializeJson(jsonData, payload);
    
    // Try to send
    WateriusHttps::ResponseData responseData = WateriusHttps::sendJsonPostRequest(
        url, sett.waterius_key, sett.waterius_email, payload);

    LOG_INFO(F("Send HTTP code:\t") << responseData.code);
    LOG_INFO(F("-- END --"));

    return responseData.code == 200;
}

#endif
#endif