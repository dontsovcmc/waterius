#ifndef OTA_MANIFEST_H
#define OTA_MANIFEST_H

#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>

#define OTA_ERR_NONE 0
#define OTA_ERR_DOWNLOAD 1
#define OTA_ERR_PARSE 2
#define OTA_ERR_INVALID_URL 3
#define OTA_ERR_FS_UPDATE 4
#define OTA_ERR_FW_UPDATE 5
#define OTA_ERR_TIMEOUT 6
#define OTA_ERR_LOW_BATTERY 7

#define OTA_MANIFEST_MAX_SIZE 1024

struct OtaManifest
{
    const char *fw_url;
    const char *fw_md5;
    size_t fw_size;
    const char *fs_url;
    const char *fs_md5;
    size_t fs_size;
    bool has_firmware;
    bool has_filesystem;
};

inline bool is_valid_ota_url(const char *url)
{
    if (!url || strlen(url) <= 8)
    {
        return false;
    }
    return strncmp(url, "https://", 8) == 0;
}

// doc должен жить пока используется manifest (zero-copy указатели)
inline uint8_t parse_manifest(const char *json, size_t len, JsonDocument &doc, OtaManifest &manifest)
{
    memset(&manifest, 0, sizeof(manifest));

    if (!json || len == 0 || len > OTA_MANIFEST_MAX_SIZE)
    {
        return OTA_ERR_PARSE;
    }

    DeserializationError err = deserializeJson(doc, json, len);
    if (err || !doc.is<JsonObject>())
    {
        return OTA_ERR_PARSE;
    }

    JsonObject root = doc.as<JsonObject>();

    if (root["firmware"].is<JsonObject>())
    {
        JsonObject fw = root["firmware"];
        manifest.fw_url = fw["url"].as<const char *>();
        manifest.fw_md5 = fw["md5"].as<const char *>();
        manifest.fw_size = fw["size"] | (size_t)0;

        if (!manifest.fw_url || !manifest.fw_md5)
        {
            return OTA_ERR_PARSE;
        }
        if (!is_valid_ota_url(manifest.fw_url))
        {
            return OTA_ERR_INVALID_URL;
        }
        manifest.has_firmware = true;
    }

    if (root["filesystem"].is<JsonObject>())
    {
        JsonObject fs = root["filesystem"];
        manifest.fs_url = fs["url"].as<const char *>();
        manifest.fs_md5 = fs["md5"].as<const char *>();
        manifest.fs_size = fs["size"] | (size_t)0;

        if (!manifest.fs_url || !manifest.fs_md5)
        {
            return OTA_ERR_PARSE;
        }
        if (!is_valid_ota_url(manifest.fs_url))
        {
            return OTA_ERR_INVALID_URL;
        }
        manifest.has_filesystem = true;
    }

    if (!manifest.has_firmware && !manifest.has_filesystem)
    {
        return OTA_ERR_PARSE;
    }

    return OTA_ERR_NONE;
}

#endif
