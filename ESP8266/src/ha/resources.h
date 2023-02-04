/**
 * @file ha_resources.h
 * @brief вспомогательный файл для хранения строк во флеше, необходимых для создания дискавери топиков
 * @version 0.1
 * @date 2023-01-22
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef HA_RESOURCES_H_
#define HA_RESOURCES_H_

#include <Arduino.h>
#include "setup.h"

#define MQTT_PARAM_COUNT 8
#define NONE -1

// строки хранятся во флэш
// https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html#how-do-i-declare-arrays-of-strings-in-progmem-and-retrieve-an-element-from-it
// чтобы массив строк хранился в флеше нужно каждую строку отдельно объявить хранящейся в кэше

/**
 * @brief объявление строк для хранения во флеше
 *
 */
static const char s_sensor[] PROGMEM = "sensor";
static const char s_number[] PROGMEM = "number";
static const char s_measurement[] PROGMEM = "measurement";
static const char s_voltage[] PROGMEM = "voltage";
static const char s_diagnostic[] PROGMEM = "diagnostic";
static const char s_v[] PROGMEM = "V";
static const char s_bvolt_name[] PROGMEM = "Battery Voltage";
static const char s_vdiff_name[] PROGMEM = "Voltage Diff";
static const char s_vdiff[] PROGMEM = "voltage_diff";
static const char s_icon_ba[] PROGMEM = "mdi:battery-alert";
static const char s_bat_low_name[] PROGMEM = "Battery Low";
static const char s_voltage_low[] PROGMEM = "voltage_low";
static const char s_battery[] PROGMEM = "battery";
static const char s_bat_name[] PROGMEM = "Battery";
static const char s_perc[] PROGMEM = "%";
static const char s_rssi_name[] PROGMEM = "RSSI";
static const char s_rssi[] PROGMEM = "rssi";
static const char s_signal_strength[] PROGMEM = "signal_strength";
static const char s_dbm[] PROGMEM = "dBm";
static const char s_resets_name[] PROGMEM = "Resets";
static const char s_resets[] PROGMEM = "resets";
static const char s_icon_cog_refresh[] PROGMEM = "mdi:cog-refresh";
static const char s_time_name[] PROGMEM = "Last seen";
static const char s_timestamp[] PROGMEM = "timestamp";
static const char s_clock[] PROGMEM = "mdi:clock";
static const char s_router_mac[] PROGMEM = "router_mac";
static const char s_router_mac_name[] PROGMEM = "Router MAC";
static const char s_mac_name[] PROGMEM = "MAC Address";
static const char s_mac[] PROGMEM = "mac";
static const char s_ip_name[] PROGMEM = "IP";
static const char s_ip[] PROGMEM = "ip";
static const char s_icon_ip[] PROGMEM = "mdi:ip-network";
static const char s_freemem_name[] PROGMEM = "Free Memory";
static const char s_freemem[] PROGMEM = "freemem";
static const char s_data_size[] PROGMEM = "data_size";
static const char s_byte[] PROGMEM = "B";
static const char s_icon_memory[] PROGMEM = "mdi:memory";
static const char s_wake_name[] PROGMEM = "Sleep Period";
static const char s_period_min[] PROGMEM = "period_min";
static const char s_duration[] PROGMEM = "duration";
static const char s_min[] PROGMEM = "min";
static const char s_config[] PROGMEM = "config";
static const char s_icon_bed_clock[] PROGMEM = "mdi:bed-clock";
static const char s_total_name[] PROGMEM = "Total";
static const char s_ch[] PROGMEM = "ch";
static const char s_total[] PROGMEM = "total";
static const char s_water[] PROGMEM = "water";
static const char s_m3[] PROGMEM = "m³";
static const char s_imp_name[] PROGMEM = "Impulses";
static const char s_imp[] PROGMEM = "imp";
static const char s_icon_pulse[] PROGMEM = "mdi:pulse";
static const char s_delta_name[] PROGMEM = "Delta";
static const char s_delta[] PROGMEM = "delta";
static const char s_icon_delta[] PROGMEM = "mdi:delta";
static const char s_adc_name[] PROGMEM = "ADC";
static const char s_adc[] PROGMEM = "adc";
static const char s_icon_counter[] PROGMEM = "mdi:counter";
static const char s_serial_name[] PROGMEM = "Serial Number";
static const char s_serial[] PROGMEM = "serial";
static const char s_icon_identifier[] PROGMEM = "mdi:identifier";
static const char s_f_name[] PROGMEM = "Factor";
static const char s_f[] PROGMEM = "f";
static const char s_icon_numeric[] PROGMEM = "mdi:numeric";

/**
 * @brief массив с ситемными сенсорами с указанием их атрибутов в MQTT
 *
 */
static const char *const GENERAL_SENSORS[][MQTT_PARAM_COUNT] PROGMEM = {
    // sensor_type, sensor_name, sensor_id, state_class, device_class,unit_of_meas,entity_category,icon
    /* Одиночные сенсоры */
    {s_sensor, s_bat_low_name, s_voltage_low, "", "", "", s_diagnostic, s_icon_ba},                               // voltage_low Батарейка разряжена >
    {s_sensor, s_bat_name, s_battery, s_measurement, s_battery, s_perc, s_diagnostic, ""},            // процент зарядки батареи
    {s_sensor, s_resets_name, s_resets, s_measurement, "", "", s_diagnostic, s_icon_cog_refresh},     // resets
    {s_sensor, s_time_name, s_timestamp, "", s_timestamp, "", s_diagnostic, s_clock},                          // Время
    /* Сенсор с атрибутами  Группа №1 */
    {s_sensor, s_bvolt_name, s_voltage, s_measurement, s_voltage, s_v, s_diagnostic, ""},             // voltage
    {s_sensor, s_vdiff_name, s_vdiff, s_measurement, s_voltage, s_v, s_diagnostic, s_icon_ba},        // Просадка напряжения voltage_diff, В
    /* Сенсор с атрибутами  Группа №2 */
    {s_sensor, s_rssi_name, s_rssi, s_measurement, s_signal_strength, s_dbm, s_diagnostic, ""},       // rssi
    {s_sensor, s_router_mac_name, s_router_mac, "", "", "", s_diagnostic, ""},                        // Мак роутера
    {s_sensor, s_mac_name, s_mac, "", "", "", s_diagnostic, ""},                                      // Мак ESP
    {s_sensor, s_ip_name, s_ip, "", "", "", s_diagnostic, s_icon_ip},                                 // IP
};


static const char *const GENERAL_NUMBERS[][MQTT_PARAM_COUNT] PROGMEM = {
    {s_number, s_wake_name, s_period_min, "", s_duration, s_min, s_config, s_icon_bed_clock},         // Настройка для автоматического добавления времени пробуждения в Home Assistant
};


/**
 * @brief массив с сенсорами для одного канала
 *
 */
static const char *const CHANNEL_SENSORS[][MQTT_PARAM_COUNT] PROGMEM = {
    // sensor_type, sensor_name, sensor_id, state_class, device_class,unit_of_meas,entity_category,icon
    /* Сенсор с атрибутами */
    {s_sensor, s_total_name, s_ch, s_total, s_water, s_m3, "", ""},                       // chN Показания
    {s_sensor, s_imp_name, s_imp, s_measurement, "", "", s_diagnostic, s_icon_pulse},     // impN Количество импульсов
    {s_sensor, s_delta_name, s_delta, s_measurement, "", "", s_diagnostic, s_icon_delta}, // deltaN Разница с предыдущими показаниями, л
    {s_sensor, s_adc_name, s_adc, s_measurement, "", "", s_diagnostic, s_icon_counter},   // adcN Аналоговый уровень
    {s_sensor, s_serial_name, s_serial, "", "", "", s_diagnostic, s_icon_identifier},     // serialN Серийный номер счетчика
    {s_sensor, s_f_name, s_f, "", "", "", s_config, s_icon_numeric},                      // fN  Вес импульса
};

static const char s_hot_wtr[] PROGMEM = "Hot Water";
static const char s_cold_wtr[] PROGMEM = "Cold Water";
/**
 * @brief Названия каналов
 *
 */
static const char *const CHANNEL_NAMES[CHANNEL_NUM] PROGMEM = {s_hot_wtr, s_cold_wtr};

static const char s_classic[] PROGMEM = "Classic";
static const char s_4c2w[] PROGMEM = "4C2W";

/**
 * @brief Название моделей
 *
 */
static const char *const MODEL_NAMES[] PROGMEM = {s_classic, s_4c2w};

#endif