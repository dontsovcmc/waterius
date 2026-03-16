#include "ota_update.h"
#include "Logging.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>

bool perform_ota_update(const JsonObject &ota, MasterI2C &masterI2C, Settings &sett, Voltage &voltage)
{
    LOG_INFO(F("OTA: start"));

    // Проверка батареи: 3 измерения с шагом 100мс, среднее
    // Отдельные замеры с паузой дают стабильное значение, т.к. АЦП ESP8266
    // шумит ±30-50 мВ, а нагрузка WiFi/передачи вызывает кратковременные
    // просадки. Пауза 100мс позволяет напряжению стабилизироваться между
    // замерами, а усреднение сглаживает выбросы.
    uint32_t sum_mv = 0;
    for (uint8_t i = 0; i < 3; i++)
    {
        if (i > 0)
        {
            delay(100);
        }
        voltage.update();
        sum_mv += voltage.value();
    }
    uint16_t avg_mv = sum_mv / 3;

    bool usb_powered = avg_mv > OTA_USB_VOLTAGE_THRESHOLD_MV;
    if (usb_powered)
    {
        LOG_INFO(F("OTA: USB power detected (") << avg_mv << F(" mV), skip battery check"));
    }
    else
    {
        LOG_INFO(F("OTA: voltage: ") << avg_mv << F(" mV (3 samples)"));
        if (avg_mv < OTA_MIN_VOLTAGE_MV)
        {
            LOG_ERROR(F("OTA: voltage too low (") << avg_mv << F(" < ") << OTA_MIN_VOLTAGE_MV << F(" mV), aborting"));
            sett.reserved9[0] = OTA_ERR_LOW_BATTERY;
            store_config(sett);
            return false;
        }
    }

    // Парсим OTA-данные из JSON-ответа сервера
    bool has_firmware = ota["firmware"].is<JsonObject>();
    bool has_filesystem = ota["filesystem"].is<JsonObject>();

    if (!has_firmware && !has_filesystem)
    {
        LOG_ERROR(F("OTA: no firmware or filesystem in ota object"));
        sett.reserved9[0] = OTA_ERR_PARSE;
        store_config(sett);
        return false;
    }

    const char *fw_url = nullptr;
    const char *fw_md5 = nullptr;
    const char *fs_url = nullptr;
    const char *fs_md5 = nullptr;

    if (has_firmware)
    {
        JsonObject fw = ota["firmware"];
        fw_url = fw["url"].as<const char *>();
        fw_md5 = fw["md5"].as<const char *>();
        size_t fw_size = fw["size"] | (size_t)0;

        if (!fw_url || !fw_md5)
        {
            LOG_ERROR(F("OTA: firmware missing url or md5"));
            sett.reserved9[0] = OTA_ERR_PARSE;
            store_config(sett);
            return false;
        }
        LOG_INFO(F("OTA: firmware url=") << fw_url << F(" md5=") << fw_md5 << F(" size=") << fw_size);
    }

    if (has_filesystem)
    {
        JsonObject fs = ota["filesystem"];
        fs_url = fs["url"].as<const char *>();
        fs_md5 = fs["md5"].as<const char *>();
        size_t fs_size = fs["size"] | (size_t)0;

        if (!fs_url || !fs_md5)
        {
            LOG_ERROR(F("OTA: filesystem missing url or md5"));
            sett.reserved9[0] = OTA_ERR_PARSE;
            store_config(sett);
            return false;
        }
        LOG_INFO(F("OTA: filesystem url=") << fs_url << F(" md5=") << fs_md5 << F(" size=") << fs_size);
    }

    // Продлеваем время бодрствования
    masterI2C.extendWakeUp();

    // Обновление filesystem (сначала FS, потом firmware)
    if (has_filesystem)
    {
        masterI2C.extendWakeUp();
        LOG_INFO(F("OTA: downloading filesystem..."));

        ESPhttpUpdate.rebootOnUpdate(false);
        ESPhttpUpdate.setMD5sum(fs_md5);

        WiFiClientSecure fs_client;
        fs_client.setInsecure();

        t_httpUpdate_return ret = ESPhttpUpdate.updateFS(fs_client, fs_url);
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
    if (has_firmware)
    {
        masterI2C.extendWakeUp();
        LOG_INFO(F("OTA: downloading firmware..."));

        ESPhttpUpdate.rebootOnUpdate(false);
        ESPhttpUpdate.setMD5sum(fw_md5);

        WiFiClientSecure fw_client;
        fw_client.setInsecure();

        t_httpUpdate_return ret = ESPhttpUpdate.update(fw_client, fw_url);
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
