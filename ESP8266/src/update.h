#ifndef _WATERIUSUPDATE_h
#define _WATERIUSUPDATE_h

#include "Logging.h"
#include <user_interface.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include "Setup.h"

bool has_new_version(const Settings &sett) 
{
    if (!strnlen(sett.update_server, UPDATE_SERVER_LEN)) {
        return false;
    }

    HTTPClient http;
    http.setTimeout(SERVER_TIMEOUT);
    http.begin(sett.update_server);

    if (http.GET() == HTTP_CODE_OK) {
        String payload = http.getString();
        int new_version = payload.toInt();
        return new_version > sett.version;
    }
    return false;
}

void update(const Settings &sett) 
{
    ESPhttpUpdate.rebootOnUpdate(false);

    String ver(sett.version);
    String url(sett.check_update);

    // 8 сек задан внутри таймаут
    t_httpUpdate_return ret = ESPhttpUpdate.update(url, ver);
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            LOG_ERROR("UPD", "failed");
            break;
        case HTTP_UPDATE_NO_UPDATES:
            LOG_ERROR("UPD", "no updates");
            break;
        case HTTP_UPDATE_OK:
            LOG_ERROR("UPD", "OK");
            break;
    }
}

#endif