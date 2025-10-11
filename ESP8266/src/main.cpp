#include <user_interface.h>
#include <umm_malloc/umm_heap_select.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "Logging.h"
#include "config.h"
#include "master_i2c.h"
#include "senders/sender_waterius.h"
#include "senders/sender_http.h"
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
AttinyData data;       // Данные от Attiny85
Settings sett;        // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; // вычисляемые данные
ADC_MODE(ADC_VCC);
Ticker voltage_ticker;

/*
Выполняется однократно при включении
*/
void setup()
{
#if WATERIUS_MODEL == WATERIUS_MODEL_MINI
            pinMode(CH0_LED_PIN, OUTPUT);
            pinMode(CH1_LED_PIN, OUTPUT);
            pinMode(BUTTON_STATE_PIN, INPUT);

            digitalWrite(CH0_LED_PIN, LOW);
            digitalWrite(CH1_LED_PIN, LOW);
#endif

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
    if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data))
    {
        // Загружаем конфигурацию из EEPROM
        config_loaded = load_config(sett);
        sett.mode = mode;
        LOG_INFO(F("Startup mode: ") << mode);

#if WATERIUS_MODEL == WATERIUS_MODEL_MINI
        if (mode == MANUAL_TRANSMIT_MODE)
        {
            pinMode(BUTTON_STATE_PIN, INPUT_PULLUP);

            unsigned long t = 0, st = millis();
            while (digitalRead(BUTTON_STATE_PIN) == LOW) {
                yield();
                t = millis() - st;
                if (t > 3000) {
                    digitalWrite(CH0_LED_PIN, (t / 300) % 2);
                    digitalWrite(CH1_LED_PIN, (t / 300) % 2);
                }
            }
            
            if (t > 3000)
            {
                mode = SETUP_MODE;
                LOG_INFO(F("Startup mode: ") << mode);
            }

            digitalWrite(CH0_LED_PIN, LOW);
            digitalWrite(CH1_LED_PIN, LOW);
            pinMode(BUTTON_STATE_PIN, INPUT);
        }
#endif

        // Вычисляем текущие показания
        calculate_values(sett, data, cdata);

        if (mode == SETUP_MODE)
        {
            LOG_INFO(F("Entering in setup mode..."));
            // Режим настройки - запускаем точку доступа на 192.168.4.1
            // Запускаем точку доступа с вебсервером

            start_active_point(sett, cdata);

            sett.setup_time = millis();
            sett.setup_finished_counter++;

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

                JsonDocument json_data; //(JSON_DYNAMIC_MSG_BUFFER);

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
                    if (!sync_ntp_time(sett)) {
                        sett.ntp_error_counter++;
                    }
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

#if WATERIUS_MODEL == WATERIUS_MODEL_MINI
    ESP.deepSleepInstant(1000000, RF_DEFAULT);    
#endif

    // { 0xC4, "Giantec Semiconductor, Inc." }, https://github.com/elitak/freeipmi/blob/master/libfreeipmi/spec/ipmi-jedec-manufacturer-identification-code-spec.c
    if (vendor_id != 0xC4) 
    {
        // Спим до следущего включения EN. (выключили Instant не ждет 92мс)
        ESP.deepSleepInstant(0, RF_DEFAULT); 
    } 
    
    while(true) yield();
}