#ifndef _WATERIUS_OTA_h
#define _WATERIUS_OTA_h


#include "ESP8266httpUpdate.h"
#include "setup.h"

void ota_update(WiFiClient& client, const char *host, const int port, const char *path)
{
    LOG_NOTICE("OTA", "Flash real size: " << ESP.getFlashChipRealSize());
    LOG_NOTICE("OTA", "Host: " << host << " port: " << port << " path: " << path);
    ESP8266HTTPUpdate u(15000);
    auto http_ret = u.update(client, host, port, path, FIRMWARE_VERSION);

    //if success reboot 

    switch (http_ret) {
        case HTTP_UPDATE_OK:
        LOG_ERROR("OTA", "OK, but no reset");
        break;
        case HTTP_UPDATE_FAILED:
        LOG_ERROR("OTA", "Update failed: (" << u.getLastError() << "), " << u.getLastErrorString().c_str());
        break;

        case HTTP_UPDATE_NO_UPDATES:
        LOG_NOTICE("OTA", "No updates");
        break;
    }
}

#endif