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
#include "json_constructor.h"

extern "C" uint32_t __crc_val;

void get_json_data(const Settings &sett, const SlaveData &data, const CalculatedData &cdata,  JsonConstructor &json)
{
  Voltage *voltage = get_voltage();
  json.begin();
  // Сведения по каналам
  json.push(F("delta0"),cdata.delta0);
  json.push(F("delta1"),cdata.delta1);
  json.push(F("ch0"),cdata.channel0, 3);
  json.push(F("ch1"),cdata.channel1, 3);
  json.push(F("imp0"),data.impulses0);
  json.push(F("imp1"),data.impulses1);
  json.push(F("f0"),sett.factor0);
  json.push(F("f1"),sett.factor1);
  json.push(F("adc0"),data.adc0);
  json.push(F("adc1"),data.adc1);
  json.push(F("serial0"),sett.serial0);
  json.push(F("serial1"),sett.serial1);

  // Battery & Voltage
  json.push(F("voltage"),voltage->average() / 1000.0,2);
  json.push(F("voltage_low"),voltage->low_voltage());
  json.push(F("voltage_diff"),(float)voltage->diff() / 1000.0,2);
  json.push(F("battery"),voltage->get_battery_level());

  // Wifi и сеть
  json.push(F("channel"),WiFi.channel());
  
  uint8_t* bssid = WiFi.BSSID();
  char router_mac[18] = { 0 };
  sprintf(router_mac, MAC_STR, bssid[0], bssid[1], bssid[2], 0, 0, 0); // последние три октета затираем

  json.push(F("router_mac"),router_mac);
  json.push(F("rssi"),WiFi.RSSI());
  json.push(F("mac"),WiFi.macAddress().c_str());
  json.pushIP(F("ip"),WiFi.localIP());
  json.push(F("dhcp"),sett.ip==0);

  // Общие сведения о приборе
  char fw_crc32[9] = { 0 };
  sprintf(fw_crc32, "%08X", __crc_val);
  json.push(F("version"),data.version);
  json.push(F("version_esp"), F(FIRMWARE_VERSION));
  json.push(F("esp_id"),getChipId());
  json.push(F("freemem"),ESP.getFreeHeap());
  json.push(F("timestamp"),get_current_time().c_str());
  json.push(F("fw_crc"),fw_crc32);

  // настройки и события
  
  json.push(F("waketime"),sett.wake_time);
  json.push(F("period_min"),sett.wakeup_per_min);
  json.push(F("setuptime"),sett.setup_time);
  json.push(F("good"),data.diagnostic);
  json.push(F("boot"),data.service);
  json.push(F("resets"),data.resets);
  json.push(F("mode"),sett.mode);
  json.push(F("setup_finished"),sett.setup_finished_counter);
  json.push(F("setup_started"),data.setup_started_counter);

  // waterius
  json.push(F("key"),sett.waterius_key);
  json.push(F("email"),sett.waterius_email);

  // Интеграции с системами
  json.push(F("mqtt"),is_mqtt(sett));
  json.push(F("blynk"),is_blynk(sett));
  json.push(F("ha"),is_ha(sett));

  json.end();

  LOG_INFO(F("JSON: Size: ") << strlen(json.c_str()));

  // JSON size 0.10.3:  355
  // JSON size 0.10.6:  439
  // JSON size 0.11: 643
}
