#include <user_interface.h>
#include <umm_malloc/umm_heap_select.h>
#include <ESP8266WiFi.h>
#include "json.h"
#include "Logging.h"
#include "config.h"
#include "core/readings.h"
#include "core/idle.h"
#include "core/wakeup.h"
#include "core/restart.h"
#include "core/alarm.h"
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
ЕСП перезагрузилась при живом питании (#354). Считается один раз на старте:
позже флаг attiny взведён всегда, и вывод по нему был бы неверным. Читает
портал - показывает плашку на главной странице.
*/
bool esp_restarted_flag = false;


/*
Пороги тревог в attiny (#202).

Уходят в каждом сеансе: у attiny они живут в ОЗУ и после снятия питания
теряются, как и период пробуждения. Пересчёт в тики - здесь, потому что вес
импульса и тип счётчика знает только ЕСП.
*/
void send_alarm_config(const Settings &sett)
{
    if (data.version < ATTINY_VER_ALARM)
    {
        return; // старая attiny тревог не умеет
    }

    const bool electro0 = sett.counter0_name == CounterName::ELECTRO;
    const bool electro1 = sett.counter1_name == CounterName::ELECTRO;

    const bool vacation = sett.vacation != 0;

    const uint16_t interval0 = alarm_interval_ticks(vacation, runtime_data.counter_type0,
                                                   sett.alarm_flow0, sett.factor0, electro0);
    const uint16_t interval1 = alarm_interval_ticks(vacation, runtime_data.counter_type1,
                                                   sett.alarm_flow1, sett.factor1, electro1);

    LOG_INFO(F("Alarm config: interval0=") << interval0 << F(" leak0=") << sett.alarm_leak0
             << F(" interval1=") << interval1 << F(" leak1=") << sett.alarm_leak1
             << F(" vacation=") << vacation);

    if (!masterI2C.setAlarmConfig(interval0, sett.alarm_leak0, interval1, sett.alarm_leak1))
    {
        LOG_ERROR(F("Alarm config wasn't set"));
    }
}

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

        esp_restarted_flag = esp_restarted((uint8_t)ESP.getResetInfoPtr()->reason,
                                           data.version, data.attiny_flags);
        LOG_INFO(F("Reset reason: ") << ESP.getResetInfoPtr()->reason
                 << F(", esp restarted: ") << esp_restarted_flag);

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

        // Сеанс по тревоге - это тоже выход на связь: в режиме "только при
        // расходе" отметка "я жив" состоялась, срок начинается заново (#361)
        if (mode == ALARM_MODE)
        {
            sett.minutes_since_send = 0;
        }

        // Вычисляем текущие показания
        calculate_values(sett, data, cdata);

        /*
        Остановка потребления (#202).

        Считается здесь по той же причине, что и расход ниже: impulses_previous
        перезапишет update_config в конце сеанса, и сравнивать будет уже не с
        чем. Поканально - "расход хоть где-то" для остановки не годится.

        Минуты набегают только в плановом пробуждении: сеанс по кнопке или по
        тревоге времени не добавляет, иначе один период засчитался бы дважды.
        Расход же обнуляет счётчик в любом режиме - он и есть событие.
        */
        const uint16_t slept_min = (mode == TRANSMIT_MODE) ? sett.wakeup_per_min : 0;

        sett.idle_min0 = update_idle_minutes(data.impulses0 != sett.impulses0_previous,
                                             sett.idle_min0, slept_min);
        sett.idle_min1 = update_idle_minutes(data.impulses1 != sett.impulses1_previous,
                                             sett.idle_min1, slept_min);

        LOG_INFO(F("Idle min: ") << sett.idle_min0 << F("/") << sett.idle_min1
                 << F(", stop: ") << consumption_stopped(sett.idle_min0, sett.alarm_stop0)
                 << F("/") << consumption_stopped(sett.idle_min1, sett.alarm_stop1));

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

            // Нужно и после сеанса: доводку до точки расписания заказываем
            // только в том сеансе, где её посчитали
            bool time_synced = false;

            if (connected)
            {
                voltage.update();
                log_system_info();

                JsonDocument json_data;
                JsonDocument json_settings_received;

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

                /*
                Данные дошли - говорим об этом attiny, иначе она будет будить
                нас снова, пока не исчерпает попытки (#202).

                Что считать "дошли", решает пользователь: alarm_confirm
                отмечает обязательных получателей. Умолчание - любой.

                В любом режиме, а не только в ALARM_MODE: состояние тревог едет
                в каждом сеансе, и плановый увозит его не хуже внепланового.
                Проверять режим здесь значило бы гонять лишний сеанс за уже
                доставленной новостью. Attiny игнорирует команду, если доклада
                не ждёт.

                Обязательно до wifi_shutdown: attiny должна узнать об этом в
                том же сеансе.
                */
                const bool confirmed = alarm_delivered(sett.alarm_confirm, status);

                // Без этой строки "почему attiny будит по кругу" не разобрать
                LOG_INFO(F("Alarm confirm: mask=") << (int)sett.alarm_confirm
                         << F(" waterius=") << (int)status.waterius
                         << F(" http=") << (int)status.http
                         << F(" mqtt=") << (int)status.mqtt
                         << F(" any=") << (int)status.delivered_any
                         << F(" -> ") << (int)confirmed);

                if (confirmed)
                {
                    masterI2C.confirmAlarm();
                }

                // Все уже отправили,  wifi не нужен - выключаем
                wifi_shutdown();

                update_config(sett, data, cdata, time_synced);
            }
            if (!must_send)
            {
                LOG_INFO(F("Idle: no consumption, WiFi stays off"));
            }

            /*
            Период заказываем в каждом сеансе, а не только после удачного
            подключения (#350). Attiny держит его в ОЗУ и после перезагрузки
            возвращается к умолчанию в 15 минут: раньше устройство без
            интернета так и будило ЕСП каждые 15 минут вместо заданного
            периода, а в молчаливом пробуждении период не уходил вниз вовсе.
            */
            send_alarm_config(sett);

            /*
            Доводка до точки расписания (period_min_tuned) - разовая: она
            зависит от того, куда целились в момент синхронизации. Заказываем
            её один раз, в том же сеансе, где посчитали, а дальше идёт целый
            период с поправкой. Иначе доводка повторяется каждым сном до
            следующей синхронизации: при периоде 15 минут устройство
            просыпалось 288 раз в сутки вместо 96.

            Нажатие кнопки задаёт расписание заново, поэтому там доводка тоже
            свежая - update_config пересчитал её в этом же сеансе. Но только
            если сеанс состоялся: без связи update_config не вызывался, и в
            period_min_tuned лежит цель прошлого цикла (#380).
            */
            const bool catchup = connected && (time_synced || mode == MANUAL_TRANSMIT_MODE);

            const uint16_t period = period_to_attiny(sett.period_min_tuned, sett.period_min_full,
                                                     catchup, sett.wakeup_per_min);
            LOG_INFO(F("Wakeup period, min (attiny):") << period);
            if (!masterI2C.setWakeUpPeriod(period))
            {
                LOG_ERROR(F("Wakeup period wasn't set"));
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

    // Коды ошибок моргаются на обеих моделях (#355). На классике светодиод
    // сидит на линии TX: на время вспышек лог замолкает, blynk_led поднимает
    // его обратно. Успех не моргается ни там, ни там: у Ватериуса2 зелёный
    // светодиод занят флешем, а на классике одна вспышка уже занята кодом
    // "села батарейка" - зелёного пина там нет, он тот же самый.
    const ErrorBlynks code = blink_code(status);
    if (code != ERROR_OK)
    {
        blynk_error(code);
    }

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