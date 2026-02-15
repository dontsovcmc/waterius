#include <user_interface.h>
#include <umm_malloc/umm_heap_select.h>
#include <ESP8266WiFi.h>
#include "json.h"
#include "Logging.h"
#include "config.h"
#include "master_i2c.h"
#include "portal/active_point.h"
#include "voltage.h"
#include "utils.h"
#include "porting.h"
#include "Ticker.h"
#include "sync_time.h"
#include "wifi_helpers.h"
#include "config.h"
#include "senders/send_data.h"
#include "ha/apply_settings.h"


MasterI2C masterI2C;     // Для общения с Attiny85 по i2c
AttinyData data;         // Данные от Attiny85 при включении
AttinyData runtime_data; // Копия данных от Attiny85. Обновляются в webportal на странице детектирования и ввода значений счётчикой.
Settings sett;           // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata;    // вычисляемые данные
ADC_MODE(ADC_VCC);
Ticker voltage_ticker;


/*
Выполняется однократно при включении
*/
void setup()
{
    LOG_BEGIN(115200); // Включаем логгирование на пине TX, 115200 8N1
    LOG_INFO(F("Waterius\n========\n"));
    LOG_INFO(F("Build: ") << __DATE__ << F(" ") << __TIME__);

    static_assert((sizeof(Settings) == 960), "sizeof Settings != 960");

    masterI2C.begin(); // Включаем i2c master

    HeapSelectIram ephemeral;
    LOG_INFO(F("IRAM free: ") << ESP.getFreeHeap() << F(" bytes"));
    {
        HeapSelectDram ephemeral;
        LOG_INFO(F("DRAM free: ") << ESP.getFreeHeap() << F(" bytes"));
    }
    LOG_INFO(F("ChipId: ") << String(getChipId(), HEX));
    LOG_INFO(F("FlashChipId: ") << String(ESP.getFlashChipId(), HEX));

    get_voltage()->begin();
    voltage_ticker.attach_ms(300, []()
                             { get_voltage()->update(); }); // через каждые 300 мс будет измеряться напряжение
}

void loop()
{
    uint8_t mode = TRANSMIT_MODE; // TRANSMIT_MODE;
    bool config_loaded = false;

    // спрашиваем у Attiny85 повод пробуждения и данные true) 
    if (masterI2C.getMode(mode) && masterI2C.getAttinyData(data))
    {   
        // Нужны в режиме настройки и при удалённой настройки
        runtime_data = data;

        // Загружаем конфигурацию из EEPROM
        config_loaded = load_config(sett);
        sett.mode = mode;
        LOG_INFO(F("Startup mode: ") << mode);

        // Вычисляем текущие показания
        calculate_values(sett, data, cdata);

        if (mode == SETUP_MODE)
        {
            LOG_INFO(F("Entering in setup mode..."));
            // Режим настройки - запускаем точку доступа на 192.168.4.1
            // Запускаем точку доступа с вебсервером
            start_active_point(sett, cdata);

            store_config(sett);

            wifi_shutdown();

            LOG_INFO(F("Set mode MANUAL_TRANSMIT to attiny"));
            masterI2C.setTransmitMode(); // Режим "Передача"

            LOG_INFO(F("Restart ESP"));
            LOG_END();

            LOG_INFO(F("Finish setup mode..."));
            ESP.restart();

            return; // сюда не должно дойти никогда
        }

        if (config_loaded)
        {
            if (wifi_connect(sett))
            {
                log_system_info();

                JsonDocument json_data;  // Передаваемый json
                JsonDocument json_settings_received; // Полученные от waterius или http server настройки

#ifndef MQTT_DISABLED
                // Подключаемся и подписываемся на мктт
                if (is_mqtt(sett))
                {
                    connect_and_subscribe_mqtt(sett, json_settings_received); //TODO remove json_data
                }
#endif
                // устанавливать время только при использовани хттпс или мктт
                if (is_mqtt(sett) || is_https(sett.waterius_host) || is_https(sett.http_url))
                {
                    if (!sync_ntp_time(sett)) {
                        sett.ntp_error_counter++;
                    }
                }

                voltage_ticker.detach(); // перестаем обновлять перед созданием объекта с данными
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
                
                send_data(sett, data, cdata, json_data, json_settings_received);

                if (settings_received(json_settings_received))
                {
                    apply_settings(json_settings_received);
                    send_data(sett, data, cdata, json_data, json_settings_received);
                }
                
                // Все уже отправили,  wifi не нужен - выключаем
                wifi_shutdown();

                update_config(sett, data, cdata);

                if (!masterI2C.setWakeUpPeriod(sett.period_min_tuned))
                {
                    LOG_ERROR(F("Wakeup period wasn't set"));
                }
            }
            store_config(sett);  // т.к. сохраняем число ошибок подключения
        }
    }
    
    if (!config_loaded)
    {
        delay(500);
        blink_led(3, 1000, 500);
    }

    LOG_INFO(F("Going to sleep"));
    LOG_END();

    uint8_t vendor_id = ESP.getFlashChipVendorId();

    masterI2C.setSleep(); // через 20мс attiny отключит EN

    // { 0xC4, "Giantec Semiconductor, Inc." }, https://github.com/elitak/freeipmi/blob/master/libfreeipmi/spec/ipmi-jedec-manufacturer-identification-code-spec.c
    if (vendor_id != 0xC4) 
    {
        ESP.deepSleepInstant(0, RF_DEFAULT); // Спим до следущего включения EN. (выключили Instant не ждет 92мс)
    } 
    
    while(true) yield();
}