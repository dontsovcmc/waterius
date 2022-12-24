#ifndef _HOMEASSISTANT_h
#define _HOMEASSISTANT_h

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "Logging.h"

/*
    Автоматическое добавления устройства в Home Assistant
*/
void homeassistant_config(PubSubClient &client, String &topic, const SlaveData &data)
{
    DynamicJsonDocument doc(MQTT_MAX_PACKET_SIZE);
    String payload;
    String deviceId = String(ESP.getChipId());
    String deviceMAC(WiFi.macAddress());

    // ch1 ХВС
    doc["name"]= "Cold water";
    doc["uniq_id"] = "waterius_ch1_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    // однократно, остальные по doc["dev"]["ids"] привяжутся
    doc["dev"]["name"] = "Waterius-" + deviceId;
    doc["dev"]["mf"] = "Waterius LLC";
    doc["dev"]["mdl"] = data.model;
    doc["dev"]["hw"] = "0";
    doc["dev"]["sw"] = String(FIRMWARE_VERSION) + "." + String(data.version);

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "ch1";
    doc["stat_cla"] = "total";
    doc["dev_cla"] = "water";
    doc["unit_of_meas"] = "m³";

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_ch1_"+ deviceId + "/config").c_str(), payload.c_str(), true);

    // ch0 ГВС
    doc["name"]= "Hot water";
    doc["uniq_id"] = "waterius_ch0_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "ch0";
    doc["stat_cla"] = "total";
    doc["dev_cla"] = "water";
    doc["unit_of_meas"] = "m³";

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_ch0_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // voltage
    doc["name"]= "Battery";
    doc["uniq_id"] = "waterius_voltage_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "voltage";
    doc["dev_cla"] = "voltage";
    doc["unit_of_meas"] = "V";
    doc["entity_category"] = "diagnostic";

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_voltage_" + deviceId + "/config").c_str(), payload.c_str(), true);         

    // RSSI
    doc["name"]= "RSSI";
    doc["uniq_id"] = "waterius_rssi_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "rssi";
    doc["dev_cla"] = "signal_strength";
    doc["unit_of_meas"] = "dBm";
    doc["entity_category"] = "diagnostic";

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("PAYLOAD: ") << payload);
    client.publish(("homeassistant/sensor/waterius_rssi_" + deviceId + "/config").c_str(), (payload).c_str(), true);

    // Resets
    doc["name"]= "Resets";
    doc["uniq_id"] = "waterius_resets_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "resets";
    doc["icon"] = "mdi:cog-refresh";

    doc["entity_category"] = "diagnostic";

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("PAYLOAD: ") << payload);
    client.publish(("homeassistant/sensor/waterius_resets_" + deviceId + "/config").c_str(), (payload).c_str(), true);


    // Настрока для автоматического добавления времени пробуждения в Home Assistant
    doc["name"]= "Wake up period";
    doc["uniq_id"] = "waterius_period_min_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["cmd_t"] = topic + "config/period_min"; // нужно устройству подписаться на этот топик или как то отслеживать
    //doc["avty_t"] = topic + "availability";
    //doc["ret"] = false;
    doc["stat_t"] = topic + "period_min";
    doc["icon"] = "mdi:bed-clock";
    //doc["unit_of_meas"] = "min";
    //doc["dev_cla"] = "duration";
    doc["max"] = 44640;
    doc["mode"] = "box";

    doc["entity_category"] = "config";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/number/waterius_period_min_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // f0  Вес импульса ГВС 
    doc["name"]= "Factor hot";
    doc["uniq_id"] = "waterius_f0_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["cmd_t"] = topic + "config/f0"; // нужно устройству подписаться на этот топик или как то отслеживать
    //doc["avty_t"] = topic + "availability";
    //doc["ret"] = false;
    doc["stat_t"] = topic + "f0";
    doc["icon"] = "mdi:format-line-weight";
    doc["max"] = 1000;
    doc["mode"] = "box";

    doc["entity_category"] = "config";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/number/waterius_f0_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // f1  Вес импульса ГВС 
    doc["name"]= "Factor cold";
    doc["uniq_id"] = "waterius_f1_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["cmd_t"] = topic + "config/f1"; //command_topic // нужно устройству подписаться на этот топик или как то отслеживать 
    //doc["avty_t"] = topic + "availability";
    //doc["ret"] = false;
    doc["stat_t"] = topic + "f1"; // state_topic // значение параметра из устройства
    doc["icon"] = "mdi:format-line-weight";
    doc["max"] = 1000;
    doc["mode"] = "box";

    doc["entity_category"] = "config";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/number/waterius_f1_" + deviceId + "/config").c_str(), (payload).c_str(), true);

    // imp0 Количество импульсов ГВС
    doc["name"]= "Impulses hot";
    doc["uniq_id"] = "waterius_imp0_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "imp0";
    doc["icon"] = "mdi:tally-mark-5";

    doc["entity_category"] = "diagnostic";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/water_pulse_imp0_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // imp1 Количество импульсов ХВС
    doc["name"]= "Impulses cold";
    doc["uniq_id"] = "waterius_imp1_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "imp1";
    doc["icon"] = "mdi:tally-mark-5";

    doc["entity_category"] = "diagnostic";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_imp1_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // delta0 Разница с предыдущими показаниями ГВС, л
    doc["name"]= "Delta hot";
    doc["uniq_id"] = "waterius_delta0_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "delta0";
    doc["icon"] = "mdi:delta";

    doc["entity_category"] = "diagnostic";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_delta0_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // delta1 Разница с предыдущими показаниями ХВС, л
    doc["name"]= "Delta cold";
    doc["uniq_id"] = "waterius_delta1_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "delta1";
    doc["icon"] = "mdi:delta";

    doc["entity_category"] = "diagnostic";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_delta1_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // adc0 Аналоговый уровень ГВС
    doc["name"]= "ADC0";
    doc["uniq_id"] = "waterius_adc0_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "adc0";
    doc["icon"] = "mdi:counter";

    doc["entity_category"] = "diagnostic";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_adc0_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // adc0 Аналоговый уровень ХВС
    doc["name"]= "ADC1";
    doc["uniq_id"] = "waterius_adc1_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "adc1";
    doc["icon"] = "mdi:counter";

    doc["entity_category"] = "diagnostic";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_adc1_" + deviceId + "/config").c_str(), payload.c_str(), true);

    // Просадка напряжения voltage_diff, В
    doc["name"]= "Voltage diff";
    doc["uniq_id"] = "waterius_voltage_diff_" + deviceId;

    doc["dev"]["ids"][0] = deviceId;
    doc["dev"]["ids"][1] = deviceMAC;

    doc["val_tpl"] = "{{ value_json }}";
    doc["stat_t"] = topic + "voltage_diff";
    doc["icon"] = "mdi:battery-alert";

    doc["dev_cla"] = "voltage";
    doc["unit_of_meas"] = "V";
    doc["entity_category"] = "diagnostic";
    doc["en"] = false;

    payload.clear();
    serializeJson(doc, payload);
    doc.clear();

    LOG_INFO(F("Payload: ") << payload);
    client.publish(("homeassistant/sensor/waterius_voltage_diff_" + deviceId + "/config").c_str(), payload.c_str(), true);

}

#endif