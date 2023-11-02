#include <user_interface.h>
#include <umm_malloc/umm_heap_select.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "Logging.h"
#include "config.h"
#include "master_i2c.h"
#include "senders/sender_waterius.h"
#include "senders/sender_http.h"
#include "senders/sender_blynk.h"
#include "senders/sender_mqtt.h"
#include "portal/active_point.h"
#include "voltage.h"
#include "utils.h"
#include "porting.h"
#include "json.h"
#include "Ticker.h"
#include "sync_time.h"
#include "wifi_helpers.h"
#include "config.h"

MasterI2C masterI2C;  // Для общения с Attiny85 по i2c
SlaveData data;       // Данные от Attiny85
Settings sett;        // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; // вычисляемые данные
ADC_MODE(ADC_VCC);
Ticker voltage_ticker;

/*
Выполняется однократно при включении
*/
void setup()
{
    LOG_BEGIN(115200); // Включаем логгирование на пине TX, 115200 8N1
    LOG_INFO(F("Booted"));

    static_assert((sizeof(Settings) == 960), "sizeof Settings != 960");

    masterI2C.begin(); // Включаем i2c master

    HeapSelectIram ephemeral;
    LOG_INFO(F("IRAM free: ") << ESP.getFreeHeap() << F(" bytes"));
    {
        HeapSelectDram ephemeral;
        LOG_INFO(F("DRAM free: ") << ESP.getFreeHeap() << F(" bytes"));
    }

    get_voltage()->begin();
    voltage_ticker.attach_ms(300, []()
                             { get_voltage()->update(); }); // через каждые 300 мс будет измеряться напряжение
}

void loop()
{
    uint8_t mode = SETUP_MODE; // TRANSMIT_MODE;
    bool config_loaded = false;

    // спрашиваем у Attiny85 повод пробуждения и данные true) 
    if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data))
    {
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

            start_active_point(sett, data, cdata);

            sett.setup_time = millis();
            sett.setup_finished_counter++;

            store_config(sett);

            wifi_shutdown();

            LOG_INFO(F("Set mode MANUAL_TRANSMIT to attiny"));
            masterI2C.sendCmd('T'); // Режим "Передача"

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

                DynamicJsonDocument json_data(JSON_DYNAMIC_MSG_BUFFER);

#ifndef MQTT_DISABLED
                // Подключаемся и подписываемся на мктт
                if (is_mqtt(sett))
                {
                    connect_and_subscribe_mqtt(sett, data, cdata, json_data);
                }
#endif
                // устанавливать время только при использовани хттпс или мктт
                if (is_mqtt(sett) || is_https(sett.waterius_host) || is_https(sett.http_url))
                {
                    sync_ntp_time(sett);
                }

                voltage_ticker.detach(); // перестаем обновлять перед созданием объекта с данными
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

                // Формироуем JSON
                get_json_data(sett, data, cdata, json_data);

                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

#ifndef WATERIUS_RU_DISABLED
                if (send_waterius(sett, json_data))
                {
                    LOG_INFO(F("HTTP: Send OK"));
                }
#endif

#ifndef HTTPS_DISABLED
                if (send_http(sett, json_data))
                {
                    LOG_INFO(F("HTTP: Send OK"));
                }
#endif

#ifndef BLYNK_DISABLED
                if (send_blynk(sett, json_data))
                {
                    LOG_INFO(F("BLYNK: Send OK"));
                }
#endif

#ifndef MQTT_DISABLED
                if (is_mqtt(sett))
                {
                    if (send_mqtt(sett, data, cdata, json_data))
                    {
                        LOG_INFO(F("MQTT: Send OK"));
                    }
                }
                else
                {
                    LOG_INFO(F("MQTT: SKIP"));
                }
#endif
                // Все уже отправили,  wifi не нужен - выключаем
                wifi_shutdown();

                update_config(sett, data, cdata);

                if (!masterI2C.setWakeUpPeriod(sett.set_wakeup))
                {
                    LOG_ERROR(F("Wakeup period wasn't set"));
                }
                else // Разбуди меня через...
                {
                    LOG_INFO(F("Wakeup period, min:") << sett.wakeup_per_min);
                    LOG_INFO(F("Wakeup period (adjusted), min:") << sett.set_wakeup);
                }

                store_config(sett);
            }
        }
    }
    LOG_INFO(F("Going to sleep"));
    LOG_END();

    if (!config_loaded)
    {
        delay(500);
        blink_led(3, 1000, 500);
    }

    masterI2C.sendCmd('Z'); // "Можешь идти спать, attiny"

    ESP.deepSleepInstant(0, RF_DEFAULT); // Спим до следущего включения EN. Instant не ждет 92мс
}