#include "json.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "setup.h"
#include "master_i2c.h"
#include "Logging.h"
#include "utils.h"
#include "porting.h"
#include "voltage.h"

/**
  Конвертирует настройки и показания в json

  @param sett настройки.
  @param data показания.
  @param cdata расчитанные показатели.
  @param voltage мониторинг питания.
  @param json_data json документ в ктороый будут записаны данные.
*/
void get_json_data(const Settings &sett, const SlaveData &data, const CalculatedData &cdata, Voltage &voltage, DynamicJsonDocument &json_data)
{
  JsonObject root = json_data.to<JsonObject>();
  // Устанавливаем данные
  root[F("delta0")] = cdata.delta0;
  root[F("delta1")] = cdata.delta1;
  root[F("good")] = data.diagnostic;
  root[F("boot")] = data.service;
  root[F("ch0")] = cdata.channel0;
  root[F("ch1")] = cdata.channel1;
  root[F("imp0")] = data.impulses0;
  root[F("imp1")] = data.impulses1;
  root[F("version")] = data.version;
  root[F("voltage")] = (float)cdata.voltage / 1000.0;
  root[F("version_esp")] = FIRMWARE_VERSION;
  root[F("key")] = sett.waterius_key;
  root[F("resets")] = data.resets;
  root[F("email")] = sett.waterius_email;
  root[F("voltage_low")] = cdata.low_voltage;
  root[F("voltage_diff")] = (float)cdata.voltage_diff / 1000.0;
  root[F("f0")] = sett.factor0;
  root[F("f1")] = sett.factor1;
  root[F("rssi")] = cdata.rssi;
  root[F("waketime")] = sett.wake_time;
  root[F("setuptime")] = sett.setup_time;
  root[F("adc0")] = data.adc0;
  root[F("adc1")] = data.adc1;
  root[F("period_min")] = sett.wakeup_per_min;
  root[F("serial0")] = sett.serial0;
  root[F("serial1")] = sett.serial1;
  root[F("mode")] = sett.mode;
  root[F("setup_finished")] = sett.setup_finished_counter;
  root[F("setup_started")] = data.setup_started_counter;
  root[F("channel")] = cdata.channel;
  root[F("router_mac")] = cdata.router_mac;
  root[F("mac")] = cdata.mac;
  root[F("ip")] = cdata.ip;
  root[F("esp_id")] = getChipId();
  root[F("freemem")] = ESP.getFreeHeap();
  root[F("timestamp")] = get_current_time();
  root[F("battery")] = voltage.get_battery_level();

  LOG_INFO(F("JSON: Mem usage: ") << json_data.memoryUsage());
  LOG_INFO(F("JSON: Size: ") << measureJson(json_data));

  // JSON size 0.10.3:  355
  // JSON size 0.10.6:  439
  // JSON size 0.10.8:
}
