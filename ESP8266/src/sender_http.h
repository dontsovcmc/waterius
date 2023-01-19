#ifndef _SENDERHTTP_h
#define _SENDERHTTP_h

#include <ESP8266WiFi.h>
#include "setup.h"
#include "master_i2c.h"
#include "WateriusHttps.h"
#include "Logging.h"
#include "json.h"

bool send_http(const Settings &sett, DynamicJsonDocument &jsonData)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send new data";
    LOG_INFO(F("HTTP: -- START -- ") << THIS_FUNC_DESCRIPTION);

    if (strnlen(sett.waterius_key, WATERIUS_KEY_LEN) == 0)
    {
        LOG_INFO(F("HTTP: SKIP"));
        return false;
    };

    if (strnlen(sett.waterius_host, WATERIUS_HOST_LEN) == 0)
    {
        LOG_INFO(F("HTTP: SKIP"));
        return false;
    }

    String url = sett.waterius_host;

    String payload = "";
    serializeJson(jsonData, payload);

    // Try to send
    WateriusHttps::ResponseData responseData = WateriusHttps::sendJsonPostRequest(
        sett.waterius_host, sett.waterius_key, sett.waterius_email, payload);

    LOG_INFO("Send HTTP code:\t" << responseData.code);
    LOG_INFO("-- END --");

    return responseData.code == 200;
}

#endif