#ifndef OTA_PARSE_H
#define OTA_PARSE_H

#include <ArduinoJson.h>

#define OTA_ERR_NONE 0
#define OTA_ERR_PARSE 1
#define OTA_ERR_FS_UPDATE 2
#define OTA_ERR_FW_UPDATE 3
#define OTA_ERR_LOW_BATTERY 4

struct OtaParams
{
    const char *fw_url;
    const char *fw_md5;
    size_t fw_size;
    const char *fs_url;
    const char *fs_md5;
    size_t fs_size;
    bool has_firmware;
    bool has_filesystem;
    uint8_t error;
};

inline OtaParams parse_ota_params(const JsonObject &ota)
{
    OtaParams p = {};

    p.has_firmware = ota["firmware"].is<JsonObject>();
    p.has_filesystem = ota["filesystem"].is<JsonObject>();

    if (!p.has_firmware && !p.has_filesystem)
    {
        p.error = OTA_ERR_PARSE;
        return p;
    }

    if (p.has_firmware)
    {
        JsonObject fw = ota["firmware"];
        p.fw_url = fw["url"].as<const char *>();
        p.fw_md5 = fw["md5"].as<const char *>();
        p.fw_size = fw["size"] | (size_t)0;

        if (!p.fw_url || !p.fw_md5)
        {
            p.error = OTA_ERR_PARSE;
            return p;
        }
    }

    if (p.has_filesystem)
    {
        JsonObject fs = ota["filesystem"];
        p.fs_url = fs["url"].as<const char *>();
        p.fs_md5 = fs["md5"].as<const char *>();
        p.fs_size = fs["size"] | (size_t)0;

        if (!p.fs_url || !p.fs_md5)
        {
            p.error = OTA_ERR_PARSE;
            return p;
        }
    }

    p.error = OTA_ERR_NONE;
    return p;
}

#endif
