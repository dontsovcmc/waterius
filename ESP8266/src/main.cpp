#include <user_interface.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "Logging.h"
#include "wifi_settings.h"
#include "master_i2c.h"
#include "setup_ap.h"
#include "sender_http.h"
#include "voltage.h"
#include "utils.h"
#include "porting.h"
#include "json.h"
#include "sender_blynk.h"
#include "sender_mqtt.h"
#include "Ticker.h"
#include "sync_time.h"
#include "wifi_helpers.h"

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

    masterI2C.begin(); // Включаем i2c master

    get_voltage()->begin();
    voltage_ticker.attach_ms(300, []()
                             { get_voltage()->update(); }); // через каждые 300 мс будет измеряться напряжение
}

/*
Берем начальные показания и кол-во импульсов,
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    LOG_INFO(F("Calculating values..."));
    LOG_INFO(F("new impulses=") << data.impulses0 << " " << data.impulses1);

    if ((sett.factor1 > 0) && (sett.factor0 > 0))
    {
        cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.factor0;
        cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.factor1;
        LOG_INFO(F("new value0=") << cdata.channel0 << F(" value1=") << cdata.channel1);

        cdata.delta0 = (data.impulses0 - sett.impulses0_previous) * sett.factor0;
        cdata.delta1 = (data.impulses1 - sett.impulses1_previous) * sett.factor1;
        LOG_INFO(F("delta0=") << cdata.delta0 << F(" delta1=") << cdata.delta1);
    }
}

void update_values(Settings &sett, const SlaveData &data, const CalculatedData &cdata)
{
    LOG_INFO(F("Updateing values...")); 
    // Сохраним текущие значения в памяти.
    sett.impulses0_previous = data.impulses0;
    sett.impulses1_previous = data.impulses1;

    // Перешлем время на сервер при след. включении
    sett.wake_time = millis();

    // Перерасчет времени пробуждения
    if (sett.mode == TRANSMIT_MODE)
    {
        // TODO: нужно удстовериться что время было устанаовлено в прошлом и сейчас
        //  т.е. перерасчет можно делать только если оба установлены или ооба не установлены
        //  т.е. time должно быть или 1970 или настоящее в обоих случаях
        //  иначе коэффициенты будут такими что никогда не выйдет на связь
        time_t now = time(nullptr);
        time_t t1 = (now - sett.last_send) / 60;
        if (t1 > 1 && data.version >= 24)
        {
            LOG_INFO(F("Minutes diff:") << t1);
            sett.set_wakeup = sett.wakeup_per_min * sett.set_wakeup / t1;
        }
        else
        {
            sett.set_wakeup = sett.wakeup_per_min;
        }
    }
    sett.last_send = time(nullptr);
}


void loop()
{
    uint8_t mode = SETUP_MODE; // TRANSMIT_MODE;

    // спрашиваем у Attiny85 повод пробуждения и данные
    if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data))
    {
        
        // Загружаем конфигурацию из EEPROM
        bool config_loaded = loadConfig(sett);
        sett.mode = mode;
        LOG_INFO(F("Startup mode: ") << mode);

        // Вычисляем текущие показания
        calculate_values(sett, data, cdata);

        if (mode == SETUP_MODE)
        {
            LOG_INFO(F("Entering in setup mode..."));
            // Режим настройки - запускаем точку доступа на 192.168.4.1
            // Запускаем точку доступа с вебсервером
            
            WiFi.persistent(false);
            WiFi.disconnect(); 

            wifi_set_mode(WIFI_AP_STA);

            setup_ap(sett, data, cdata);
            
            wifi_shutdown();

            LOG_INFO(F("Set mode MANUAL_TRANSMIT to attiny"));
            masterI2C.sendCmd('T'); // Режим "Передача"

            LOG_INFO(F("Restart ESP"));
            LOG_END();

            wifi_set_mode(WIFI_OFF);
            LOG_INFO(F("Finish setup mode..."));
            ESP.restart();

            return; // сюда не должно дойти никогда
        }

        if (config_loaded)
        {
            if (wifi_connect(sett))
            {
                log_system_info();

                String mqtt_topic;
                DynamicJsonDocument json_data(JSON_DYNAMIC_MSG_BUFFER);

                // Подключаемся и подписываемся на мктт
                if (is_mqtt(sett))
                {
                    connect_and_subscribe_mqtt(sett, data, cdata, json_data);
                }

                // устанавливать время только при использовани хттпс или мктт
                if (is_mqtt(sett) || is_https(sett.waterius_host))
                {
                    sync_ntp_time();
                }

                voltage_ticker.detach(); // перестаем обновлять перед созданием объекта с данными
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

                // Формироуем JSON
                get_json_data(sett, data, cdata, json_data);

                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

#ifndef HTTPS_DISABLED
                if (send_http(sett, json_data))
                {
                    LOG_INFO(F("HTTP: Send OK"));
                }
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
#endif

#ifndef BLYNK_DISABLED
                if (send_blynk(sett, json_data))
                {
                    LOG_INFO(F("BLYNK: Send OK"));
                }
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
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

                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
#endif
                // Все уже отправили,  wifi не нужен - выключаем
                wifi_shutdown();

                update_values(sett, data, cdata);

                if (!masterI2C.setWakeUpPeriod(sett.set_wakeup))
                {
                    LOG_ERROR(F("Wakeup period wasn't set"));
                }
                else //"Разбуди меня через..."
                {
                    LOG_INFO(F("Wakeup period, min:") << sett.wakeup_per_min);
                    LOG_INFO(F("Wakeup period, tick:") << sett.set_wakeup);
                }

                storeConfig(sett);
            }
        }
    }
    LOG_INFO(F("Going to sleep"));
    masterI2C.sendCmd('Z'); // "Можешь идти спать, attiny"
    LOG_END();
    ESP.deepSleepInstant(0, RF_DEFAULT); // Спим до следущего включения EN. Instant не ждет 92мс
}