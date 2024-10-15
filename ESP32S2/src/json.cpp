#include "json.h"
#include <ArduinoJson.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#endif
#include "setup.h"
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "porting.h"
#include "voltage.h"
#include "sync_time.h"
#include "wifi_helpers.h"


void get_json_data(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, DynamicJsonDocument &json_data)
{
    Voltage *voltage = get_voltage();
    JsonObject root = json_data.to<JsonObject>();
    // Сведения по каналам
    root[F("delta0")] = cdata.delta0;
    root[F("delta1")] = cdata.delta1;
    root[F("ch0")] = cdata.channel0;
    root[F("ch1")] = cdata.channel1;
    root[F("ch0_start")] = sett.channel0_start;
    root[F("ch1_start")] = sett.channel1_start;
    root[F("imp0")] = data.impulses0;
    root[F("imp1")] = data.impulses1;
    root[F("f0")] = sett.factor0;
    root[F("f1")] = sett.factor1;
    root[F("adc0")] = data.adc0;
    root[F("adc1")] = data.adc1;
    root[F("serial0")] = sett.serial0;
    root[F("serial1")] = sett.serial1;
    root[F("itype0")] = data.counter_type0;
    root[F("itype1")] = data.counter_type1;
    root[F("cname0")] = sett.counter0_name;
    root[F("cname1")] = sett.counter1_name;
    root[F("data_type0")] = (uint8_t)data_type_by_name(sett.counter0_name);
    root[F("data_type1")] = (uint8_t)data_type_by_name(sett.counter1_name);

    // Battery & Voltage
    root[F("voltage")] = voltage->average() / 1000.0;
    root[F("voltage_low")] = voltage->low_voltage();
    root[F("voltage_diff")] = (float)voltage->diff() / 1000.0;
    root[F("battery")] = voltage->get_battery_level();

    // Wifi и сеть
    root[F("channel")] = WiFi.channel();

    wifi_phy_mode_t pmode;
    esp_err_t ret = esp_wifi_sta_get_negotiated_phymode(&pmode);

    root[F("wifi_phy_mode")] = wifi_phy_mode_title(pmode); 
    root[F("wifi_phy_mode_s")] = wifi_phy_mode_title((wifi_phy_mode_t)sett.wifi_phy_mode);

    uint8_t *bssid = WiFi.BSSID();
    char router_mac[18] = {0};
    sprintf(router_mac, MAC_STR, bssid[0], bssid[1], bssid[2], 0, 0, 0); // последние три октета затираем

    root[F("router_mac")] = String(router_mac);
    root[F("rssi")] = WiFi.RSSI();
    root[F("mac")] = WiFi.macAddress();
    root[F("ip")] = WiFi.localIP();
    root[F("dhcp")] = sett.ip == 0;

    // Общие сведения о приборе
    root[F("version")] = data.version;
    root[F("version_esp")] = FIRMWARE_VERSION;
    root[F("esp_id")] = getChipId();
    root[F("freemem")] = ESP.getFreeHeap();
    root[F("timestamp")] = get_current_time();

    // настройки и события
    root[F("waketime")] = sett.wake_time;
    root[F("period_min_tuned")] = sett.set_wakeup;
    root[F("period_min")] = sett.wakeup_per_min;
    root[F("setuptime")] = sett.setup_time;
    root[F("boot")] = data.service;
    root[F("resets")] = data.resets;
    root[F("mode")] = sett.mode;
    root[F("setup_finished")] = sett.setup_finished_counter;
    root[F("setup_started")] = data.setup_started_counter;
    root[F("ntp_errors")] = sett.ntp_error_counter;

    // waterius
    root[F("key")] = sett.waterius_key;
    root[F("email")] = sett.waterius_email;

    // Интеграции с системами
    root[F("mqtt")] = is_mqtt(sett);
    root[F("ha")] = is_ha(sett);
    root[F("http")] = is_http(sett);

    LOG_INFO(F("JSON: Mem usage: ") << json_data.memoryUsage());
    LOG_INFO(F("JSON: Size: ") << measureJson(json_data));

    // JSON size 0.10.3: 355
    // JSON size 0.10.6: 439
    // JSON size 0.11: 643
    // JSON size 0.11.4: 722 JSON: Mem usage: 1168
    // JSON size 1.0.1 727 JSON: Mem usage: 1168  //no mqtt
}
