#include "wifi_helpers.h"
#include "Logging.h"
#include "utils.h"
#include <LittleFS.h>
#include <ESP8266WiFiScan.h>

#define WIFI_SET_MODE_ATTEMPTS 2

// Adoption of Tasmota wifi module
// https://github.com/arendst/Tasmota/blob/development/tasmota/tasmota_support/support_wifi.ino

void wifi_set_mode(WiFiMode_t wifi_mode)
{
    if (WiFi.getMode() == wifi_mode)
    {
        return;
    }

    if (wifi_mode != WIFI_OFF)
    {
        WiFi.forceSleepWake();
        delay(100);
    }

    uint32_t attempts = WIFI_SET_MODE_ATTEMPTS;
    while (!WiFi.mode(wifi_mode) && attempts--)
    {
        LOG_INFO(F("WIFI: Retry set Mode..."));
        delay(100);
    }

    if (wifi_mode == WIFI_OFF)
    {
        delay(1000); // TODO: наверное нужно уменьшить до 100-300 мсек
        WiFi.forceSleepBegin();
        delay(1);
    }
    else
    {
        delay(30); // Must allow for some time to init.
    }
}

void wifi_begin(Settings &sett, WiFiMode_t wifi_mode)
{

    WiFi.persistent(false); // Solve possible wifi init errors (re-add at 6.2.1.16 #4044, #4083)
    WiFi.disconnect(true);  // Delete SDK wifi config
    LOG_INFO(F("WIFI: disconnect"));

    delay(200); // подождем чтобы проинициализировалась сеть

    wifi_set_mode(wifi_mode); // Disable AP mode
    if (sett.wifi_phy_mode)
    {
        if (!WiFi.setPhyMode((WiFiPhyMode_t)sett.wifi_phy_mode))
        {
            LOG_ERROR(F("WIFI: Failed set phy mode ") << sett.wifi_phy_mode);
        }
    }

    if (!WiFi.getAutoConnect()) // Tasmota..
    {
        WiFi.setAutoConnect(true);
        LOG_INFO(F("WIFI: set auto connect true"));
    }

    if (!is_dhcp(sett))
    {
        LOG_INFO(F("WIFI: use static IP"));
        IPAddress fallback_dns_server;
        fallback_dns_server.fromString(DEF_FALLBACK_DNS);
        WiFi.config(sett.ip, sett.gateway, sett.mask, sett.gateway, fallback_dns_server);
    }

    if (!WiFi.hostname(get_device_name())) // ESP8266 needs this here (after WiFi.mode)
    {
        LOG_ERROR(F("WIFI: set hostname failed"));
    }

    delay(100); // подождем чтобы проинициализировалась сеть

    if (sett.wifi_channel)
    {
        LOG_INFO(F("WIFI: begin channel: ") << sett.wifi_channel);
        WiFi.begin(sett.wifi_ssid, sett.wifi_password, sett.wifi_channel, sett.wifi_bssid);
    }
    else
    {
        LOG_INFO(F("WIFI: begin"));
        WiFi.begin(sett.wifi_ssid, sett.wifi_password);
    }

    int8_t ret = WiFi.waitForConnectResult(ESP_CONNECT_TIMEOUT); //        ждем результата подключения
    if (ret == -1) 
    {
        LOG_ERROR(F("WIFI: connect timeout"));
    } 
    else 
    {
        LOG_INFO(F("WIFI: status=") << WiFi.status());
    }
}

void wifi_shutdown()
{
    WiFi.disconnect(true);
    delay(100);
    yield();
    wifi_set_mode(WIFI_OFF);
}

String wifi_phy_mode_title(const WiFiPhyMode_t m)
{
    // WiFi.setPhyMode(WIFI_PHY_MODE_11B = 1, WIFI_PHY_MODE_11G = 2, WIFI_PHY_MODE_11N = 3);
    switch (m)
    {
    case WIFI_PHY_MODE_11B:
        return F("B");
    case WIFI_PHY_MODE_11G:
        return F("G");
    case WIFI_PHY_MODE_11N:
        return F("N");
    default:
        return String((int)m);
    }
}


bool wifi_connect(Settings &sett, WiFiMode_t wifi_mode /*= WIFI_STA*/)
{
    uint32_t start_time = millis();
    LOG_INFO(F("WIFI: Connecting..."));
    int attempts = WIFI_CONNECT_ATTEMPTS;
    do
    {
        LOG_INFO(F("WIFI: Attempt #") << WIFI_CONNECT_ATTEMPTS - attempts + 1 << F(" from ") << WIFI_CONNECT_ATTEMPTS);
        wifi_begin(sett, wifi_mode);
        if (WiFi.isConnected())
        {
            sett.wifi_channel = WiFi.channel(); // сохраняем для быстрого коннекта
            uint8_t *bssid = WiFi.BSSID();
            memcpy((void *)&sett.wifi_bssid, (void *)bssid, sizeof(sett.wifi_bssid)); // сохраняем для быстрого коннекта
            LOG_INFO(F("WIFI: Connected."));
            LOG_INFO(F("WIFI: SSID: ") << WiFi.SSID() 
                << F(" Channel: ") << WiFi.channel() 
                << F(" BSSID: ") << WiFi.BSSIDstr()
                << F(" mode: ") << wifi_phy_mode_title(WiFi.getPhyMode()));

            LOG_INFO(F("WIFI: Time spent ") << millis() - start_time << F(" ms"));
            return true;
        }
        sett.wifi_channel = 0;
        LOG_ERROR(F("WIFI: Connection failed."));
    } while (--attempts);

    LOG_ERROR(F("WIFI: Connection failed.") << millis() - start_time << F(" ms"));
    return false;
}

/* Важно: вызывать функцию, после сканирования WiFi сетей */
void write_ssid_to_file()
{
    // LittleFS.remove("/ssid.txt");
    File file = LittleFS.open("/ssid.txt", "w");
    if (!file)
    {
        LOG_ERROR(F("FS: Failed to open ssid.txt for writing"));
        return;
    }

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED)
    {
        if (!file.print(F("No WIFI networks found")))
        {
            LOG_ERROR(F("FS: Failed to write ssid.txt"));
        }
    }
    else if (n)
    {
        for (int i = 0; i < n; ++i)
        {
            const bss_info *it = WiFi.getScanInfoByIndex(i);
            char tmp[33];
            String s;

            if (it)
            {
                memcpy(tmp, it->ssid, sizeof(it->ssid));
                tmp[32] = 0;

                file.printf("\nSSID:%s\n", tmp);
                file.printf("bssid:%02x%02x%02x%02x%02x%02x\n",
                            it->bssid[0], it->bssid[1], it->bssid[2],
                            it->bssid[3], it->bssid[4], it->bssid[5]);
                file.printf("ssid_len:%d\n", it->ssid_len);
                file.printf("channel:%d\n", it->channel);
                file.printf("rssi:%d\n", it->rssi);
                file.printf("authmode:%d\n", (int)it->authmode);
                file.printf("is_hidden:%d\n", it->is_hidden);
                file.printf("freq_offset:%d\n", it->freq_offset);
                file.printf("freqcal_val:%d\n", it->freqcal_val);
                // file.printf("freqcal_val:%d\n",it->freqcal_val);        //uint8 *esp_mesh_ie;
                file.printf("simple_pair:%d\n", it->simple_pair);
                file.printf("pairwise_cipher:%d\n", it->pairwise_cipher);
                file.printf("group_cipher:%d\n", it->group_cipher);
                file.printf("phy_11b:%d\n", it->phy_11b);
                file.printf("phy_11g:%d\n", it->phy_11g);
                file.printf("phy_11n:%d\n", it->phy_11n);
                file.printf("wps:%d\n", it->wps);
                file.printf("reserved:%u\n=============", it->reserved); // uint32_t reserved:28;
            }
        }
    }

    delay(50); // Make sure the CREATE and LASTWRITE times are different
    file.close();
}