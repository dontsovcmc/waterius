#include "ota_update.h"
#include "ota_manifest.h"
#include "Logging.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include "ESP8266HTTPClient.h"
#include <ESP8266httpUpdate.h>

bool perform_ota_update(const char *manifest_url, MasterI2C &masterI2C, Settings &sett, Voltage &voltage)
{
    LOG_INFO(F("OTA: start"));
    LOG_INFO(F("OTA: Manifest URL: ") << manifest_url);

    // Проверка батареи (пропускаем при USB-питании: voltage > 4500 мВ)
    uint16_t avg_mv = voltage.average();
    bool usb_powered = avg_mv > OTA_USB_VOLTAGE_THRESHOLD_MV;
    if (usb_powered)
    {
        LOG_INFO(F("OTA: USB power detected (") << avg_mv << F(" mV), skip battery check"));
    }
    else
    {
        uint8_t battery = voltage.get_battery_level();
        LOG_INFO(F("OTA: battery level: ") << battery << F("%, voltage: ") << avg_mv << F(" mV"));
        if (avg_mv < OTA_MIN_VOLTAGE_MV)
        {
            LOG_ERROR(F("OTA: voltage too low (") << avg_mv << F(" < ") << OTA_MIN_VOLTAGE_MV << F(" mV), aborting"));
            sett.reserved9[0] = OTA_ERR_LOW_BATTERY;
            store_config(sett);
            return false;
        }
        if (battery < OTA_MIN_BATTERY_PERCENT)
        {
            LOG_ERROR(F("OTA: battery too low, aborting"));
            sett.reserved9[0] = OTA_ERR_LOW_BATTERY;
            store_config(sett);
            return false;
        }
    }

    // Продлеваем время бодрствования
    masterI2C.extendWakeUp();

    // Скачиваем манифест (в блоке, чтобы WiFiClientSecure освободил ~15 КБ до скачивания прошивки)
    String body;
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setTimeout(10000);
        http.setReuse(false);

        if (!http.begin(client, manifest_url))
        {
            LOG_ERROR(F("OTA: failed to connect for manifest"));
            sett.reserved9[0] = OTA_ERR_DOWNLOAD;
            store_config(sett);
            return false;
        }

        int code = http.GET();
        if (code != 200)
        {
            LOG_ERROR(F("OTA: manifest HTTP code: ") << code);
            http.end();
            sett.reserved9[0] = OTA_ERR_DOWNLOAD;
            store_config(sett);
            return false;
        }

        body = http.getString();
        http.end();
    }

    LOG_INFO(F("OTA: manifest size: ") << body.length());

    // Парсим манифест
    JsonDocument doc;
    OtaManifest manifest;
    uint8_t err = parse_manifest(body.c_str(), body.length(), doc, manifest);
    body = String(); // освобождаем память
    if (err != OTA_ERR_NONE)
    {
        LOG_ERROR(F("OTA: manifest parse error: ") << err);
        sett.reserved9[0] = err;
        store_config(sett);
        return false;
    }

    // Логируем содержимое манифеста
    if (manifest.has_firmware)
    {
        LOG_INFO(F("OTA: firmware url=") << manifest.fw_url << F(" md5=") << manifest.fw_md5 << F(" size=") << manifest.fw_size);
    }
    if (manifest.has_filesystem)
    {
        LOG_INFO(F("OTA: filesystem url=") << manifest.fs_url << F(" md5=") << manifest.fs_md5 << F(" size=") << manifest.fs_size);
    }

    // Обновление filesystem (сначала FS, потом firmware)
    if (manifest.has_filesystem)
    {
        masterI2C.extendWakeUp();
        LOG_INFO(F("OTA: downloading filesystem..."));

        ESPhttpUpdate.rebootOnUpdate(false);
        ESPhttpUpdate.setMD5sum(manifest.fs_md5);

        WiFiClientSecure fs_client;
        fs_client.setInsecure();

        t_httpUpdate_return ret = ESPhttpUpdate.updateFS(fs_client, manifest.fs_url);
        if (ret != HTTP_UPDATE_OK)
        {
            LOG_ERROR(F("OTA: filesystem update failed: ") << ESPhttpUpdate.getLastErrorString());
            sett.reserved9[0] = OTA_ERR_FS_UPDATE;
            store_config(sett);
            return false;
        }
        LOG_INFO(F("OTA: filesystem updated OK"));
    }

    // Обновление firmware
    if (manifest.has_firmware)
    {
        masterI2C.extendWakeUp();
        LOG_INFO(F("OTA: downloading firmware..."));

        ESPhttpUpdate.rebootOnUpdate(false);
        ESPhttpUpdate.setMD5sum(manifest.fw_md5);

        WiFiClientSecure fw_client;
        fw_client.setInsecure();

        t_httpUpdate_return ret = ESPhttpUpdate.update(fw_client, manifest.fw_url);
        if (ret != HTTP_UPDATE_OK)
        {
            LOG_ERROR(F("OTA: firmware update failed: ") << ESPhttpUpdate.getLastErrorString());
            sett.reserved9[0] = OTA_ERR_FW_UPDATE;
            store_config(sett);
            return false;
        }
        LOG_INFO(F("OTA: firmware updated OK"));
    }

    LOG_INFO(F("OTA: complete, restarting ESP..."));
    LOG_END();
    ESP.restart();

    return true;
}
