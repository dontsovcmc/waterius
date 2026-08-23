#include <user_interface.h>
#include <umm_malloc/umm_heap_select.h>
#include <ESP8266WiFi.h>
#include "json.h"
#include "Logging.h"
#include "config.h"
#include "core/readings.h"
#include "core/idle.h"
#include "master_i2c.h"
#include "senders/send_data.h"
#include "ha/apply_settings.h"
#include "portal/active_point.h"
#include "voltage.h"
#include "utils.h"
#include "porting.h"
#include "sync_time.h"
#include "wifi_helpers.h"
#include "config.h"
#include "wleds.h"
#include "ota_update.h"
#include "flash_reset.h"

MasterI2C masterI2C;     // Для общения с Attiny85 по i2c
AttinyData data;         // Данные от Attiny85 при включении
AttinyData runtime_data; // Копия данных от Attiny85. Обновляются в webportal на странице детектирования и ввода значений счётчиков.
Settings sett;           // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata;    // вычисляемые данные
ADC_MODE(ADC_VCC);
Voltage voltage;


/*
Выполняется однократно при включении
*/
void setup()
{
    setup_leds();

    LOG_BEGIN(LOG_BAUD); // Включаем логгирование на пине TX, 115200 8N1
    LOG_INFO(F("Waterius\n========\n"));
    LOG_INFO(F("Build: ") << __DATE__ << F(" ") << __TIME__);

    // static_assert на размер Settings — в core/types.h, рядом с самой структурой

    masterI2C.begin(); // Включаем i2c master

    HeapSelectIram ephemeral;
    LOG_INFO(F("IRAM free: ") << ESP.getFreeHeap() << F(" bytes"));
    {
        HeapSelectDram ephemeral;
        LOG_INFO(F("DRAM free: ") << ESP.getFreeHeap() << F(" bytes"));
    }
    LOG_INFO(F("ChipId: ") << String(getChipId(), HEX));
    LOG_INFO(F("FlashChipId: ") << String(ESP.getFlashChipId(), HEX));
    LOG_INFO(F("ESP firmware ver: ") << FIRMWARE_VERSION);
}

void loop()
{
    uint8_t mode = TRANSMIT_MODE; // TRANSMIT_MODE;
    bool config_loaded = false;
    SessionStatus status; // чем закончился сеанс — этим моргнём перед сном

    // спрашиваем у Attiny85 повод пробуждения и данные true)
    if (masterI2C.getMode(mode) && masterI2C.getAttinyData(data))
    {
        runtime_data = data;

        voltage.update();
#if WATERIUS_MODEL == WATERIUS_MODEL_2
        if (mode == MANUAL_TRANSMIT_MODE)
        {
            mode = wait_button_release();
        }
        if (mode == SETUP_MODE) // Если режим "Настройка"
        {
            masterI2C.setSetupMode(); 
        }
#endif
        // Загружаем конфигурацию из EEPROM
        config_loaded = load_config(sett);
        sett.mode = mode;
        LOG_INFO(F("Startup mode: ") << mode);

        // Пробуждение по таймеру — ещё один проспанный период. Считаем их,
        // потому что время между синхронизациями неизвестно, а число
        // заказанных периодов известно точно (#357).
        if (mode == TRANSMIT_MODE)
        {
            sett.wakeups_since_sync = bump_wakeups(sett.wakeups_since_sync);
            sett.minutes_since_send = add_minutes(sett.minutes_since_send, sett.wakeup_per_min);
        }

        // Вычисляем текущие показания
        calculate_values(sett, data, cdata);

        /*
        Режим "выходить на связь только при расходе воды" (#361).

        Просыпаемся как обычно, но сеанс Wi-Fi затеваем, только если импульсы
        пошли или пора отметиться, что устройство живо. Сеанс и есть главная
        статья расхода батареи: секунды работы радио против долей секунды на
        чтение attiny по i2c.

        Сравнивать импульсы надо здесь: impulses_previous перезапишет
        update_config в конце сеанса.
        */
        bool must_send = true;

        if (mode == TRANSMIT_MODE && sett.send_on_consumption)
        {
            const bool consumed = consumption_detected(data.impulses0, sett.impulses0_previous,
                                                       data.impulses1, sett.impulses1_previous);
            must_send = need_transmit(true, consumed, sett.minutes_since_send);

            LOG_INFO(F("Idle: consumed=") << consumed
                     << F(", silence_min=") << sett.minutes_since_send
                     << F(", transmit=") << must_send);

            // Сбрасываем на попытке, а не на успехе: устройство без интернета
            // иначе долбилось бы в сеть каждое пробуждение до конца батареи
            if (must_send)
            {
                sett.minutes_since_send = 0;
            }
        }

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
            // Молчаливое пробуждение (режим "только при расходе") роутер не
            // трогает и ошибкой связи не считается
            const bool connected = must_send && wifi_connect(sett);
            status.wifi_connected = !must_send || connected;

            if (connected)
            {
                voltage.update();
                log_system_info();

                JsonDocument json_data;
                JsonDocument json_settings_received;
                bool time_synced = false;

                // Подключаемся и подписываемся на мктт
#ifndef MQTT_DISABLED
                if (is_mqtt(sett))
                {
                    if (!connect_and_subscribe_mqtt(sett, json_settings_received))
                    {
                        status.mqtt = SEND_NO_CONNECTION;
                    }
                }
#endif

                // устанавливать время только при использовани хттпс или мктт
                if (is_mqtt(sett) || is_https(sett.waterius_host) || is_https(sett.http_url))
                {
                    time_synced = maybe_sync_time(sett);
                }

                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

                send_data(sett, data, cdata, json_data, json_settings_received, status);

                if (sett.ota_error != OTA_ERR_NONE)
                {
                    sett.ota_error = OTA_ERR_NONE;
                    store_config(sett);
                }

                if (settings_received(json_settings_received))
                {
                    apply_settings(json_settings_received, sett, data, cdata);

                    // Типы входов команда меняет в attiny и в runtime_data, а
                    // payload собирается из снимка data. Без переноса наверх
                    // уходили бы прежние значения, и селектор в Home Assistant
                    // отщёлкивал бы обратно (#360).
                    apply_counter_types(data, runtime_data);

                    send_data(sett, data, cdata, json_data, json_settings_received, status);
                }

#if WATERIUS_MODEL == WATERIUS_MODEL_2
                if (has_ota(json_settings_received))
                {
                    perform_ota_update(json_settings_received[F("ota")].as<JsonObject>(), masterI2C, sett, voltage);
                }
#endif

                // Все уже отправили,  wifi не нужен - выключаем
                wifi_shutdown();

                update_config(sett, data, cdata, time_synced);

                if (!masterI2C.setWakeUpPeriod(sett.period_min_tuned))
                {
                    LOG_ERROR(F("Wakeup period wasn't set"));
                }
            }
            if (!must_send)
            {
                LOG_INFO(F("Idle: no consumption, WiFi stays off"));
            }

            // Сохраняем всегда: даже в молчаливом пробуждении изменились
            // счётчики, а без них не наступит ни срок отметки, ни срок NTP
            store_config(sett);  // т.к. сохраняем число ошибок подключения
        }
    }

    // config_loaded остаётся false и когда attiny не ответил по i2c: код
    // ошибки тот же, что и при испорченной конфигурации
    status.config_loaded = config_loaded;
    status.low_voltage = voltage.low_voltage();

#if WATERIUS_MODEL == WATERIUS_MODEL_2
    // Ватериус2 — единственная модель со светодиодами: на классике оба пина
    // указывают на TX, и индикацией там занимается attiny.
    //
    // Успех не моргаем: GREEN_LED_PIN = 19, а у ESP8266 выводов больше 16 нет,
    // так что зелёная вспышка — это просто 200 мс лишнего бодрствования
    const ErrorBlynks code = blink_code(status);
    if (code != ERROR_OK)
    {
        blynk_error(code);
    }
#else
    if (!status.config_loaded)
    {
        blynk_error(ERROR_CONFIG);
    }
#endif

    LOG_INFO(F("Going to sleep"));
    LOG_END();

    uint8_t vendor_id = ESP.getFlashChipVendorId();

    masterI2C.setSleep(); // через 20мс attiny отключит EN

    release_leds();

#if WATERIUS_MODEL == WATERIUS_MODEL_1
    // { 0xC4, "Giantec Semiconductor, Inc." }, https://github.com/elitak/freeipmi/blob/master/libfreeipmi/spec/ipmi-jedec-manufacturer-identification-code-spec.c
    if (vendor_id != 0xC4)
    {
        // JEDEC software reset SPI flash (0x66+0x99) перед снятием EN.
        // Workaround для модулей с залипающей flash (BoyaMicro 25Q80ES*, vendor 0x68).
        flash_software_reset();

        // Спим до следущего включения EN. (выключили Instant не ждет 92мс)
        ESP.deepSleepInstant(0, RF_DEFAULT);
    }
#endif
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    ESP.deepSleepInstant(1000000, RF_DEFAULT);
#endif

    while(true) yield();
}