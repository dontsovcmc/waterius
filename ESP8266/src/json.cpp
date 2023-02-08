#include "json.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "setup.h"
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "porting.h"
#include "voltage.h"
#include "sync_time.h"

void get_json_data(const Settings &sett, const SlaveData &data, const CalculatedData &cdata,  DynamicJsonDocument &json_data)
{
  Voltage *voltage = get_voltage();
  JsonObject root = json_data.to<JsonObject>();
  // Сведения по каналам
  root[F("delta0")] = cdata.delta0;
  root[F("delta1")] = cdata.delta1;
  root[F("ch0")] = cdata.channel0;
  root[F("ch1")] = cdata.channel1;
  root[F("imp0")] = data.impulses0;
  root[F("imp1")] = data.impulses1;
  root[F("f0")] = sett.factor0;
  root[F("f1")] = sett.factor1;
  root[F("adc0")] = data.adc0;
  root[F("adc1")] = data.adc1;
  root[F("serial0")] = sett.serial0;
  root[F("serial1")] = sett.serial1;

  // Battery & Voltage
  root[F("voltage")] = voltage->average() / 1000.0;
  root[F("voltage_low")] = voltage->low_voltage();
  root[F("voltage_diff")] = (float)voltage->diff() / 1000.0;
  root[F("battery")] = voltage->get_battery_level();

  // Wifi и сеть
  root[F("channel")] = cdata.channel;
  root[F("router_mac")] = cdata.router_mac;
  root[F("rssi")] = cdata.rssi;
  root[F("mac")] = cdata.mac;
  root[F("ip")] = cdata.ip;
  root[F("dhcp")] = sett.ip==0;

  // Общие сведения о приборе
  root[F("version")] = data.version;
  root[F("version_esp")] = FIRMWARE_VERSION;
  root[F("esp_id")] = getChipId();
  root[F("freemem")] = ESP.getFreeHeap();
  root[F("timestamp")] = get_current_time();

  // настройки и события
  root[F("waketime")] = sett.wake_time;
  root[F("period_min")] = sett.wakeup_per_min;
  root[F("setuptime")] = sett.setup_time;
  root[F("good")] = data.diagnostic;
  root[F("boot")] = data.service;
  root[F("resets")] = data.resets;
  root[F("mode")] = sett.mode;
  root[F("setup_finished")] = sett.setup_finished_counter;
  root[F("setup_started")] = data.setup_started_counter;

  // waterius
  root[F("key")] = sett.waterius_key;
  root[F("email")] = sett.waterius_email;

  // Интеграции с системами
  root[F("mqtt")] = is_mqtt(sett);
  root[F("blynk")] = is_blynk(sett);
  root[F("ha")] = is_ha(sett);
  

  LOG_INFO(F("JSON: Mem usage: ") << json_data.memoryUsage());
  LOG_INFO(F("JSON: Size: ") << measureJson(json_data));

  // JSON size 0.10.3:  355
  // JSON size 0.10.6:  439
  // JSON size 0.11: 643
}
