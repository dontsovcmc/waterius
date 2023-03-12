#include <user_interface.h>
#include <ESP8266WiFi.h>
#include "LittleFS.h"
#include "Logging.h"
#include "config.h"
#include "master_i2c.h"
// #include "setup_ap.h"
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
#include "config.h"
#include "portal.h"
#include "json_constructor.h"
#include "DNSServer.h"

MasterI2C masterI2C;  // Для общения с Attiny85 по i2c
SlaveData data;       // Данные от Attiny85
Settings sett;        // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; // вычисляемые данные
ADC_MODE(ADC_VCC);
Ticker voltage_ticker;
volatile bool periodUpdated = false;

extern "C" uint32_t __crc_val;

/*
Выполняется однократно при включении
*/
void setup()
{
    LOG_BEGIN(115200); // Включаем логгирование на пине TX, 115200 8N1
    LOG_INFO(F("Booted"));
    char firmware_crc32[9] = {0};
    sprintf(firmware_crc32, "%08X", __crc_val);
    LOG_INFO(F("Firmware CRC32: ") << firmware_crc32);

    masterI2C.begin(); // Включаем i2c master
    LittleFS.begin();

    get_voltage()->begin();
    voltage_ticker.attach_ms(300, []()
                             { get_voltage()->update(); }); // через каждые 300 мс будет измеряться напряжение
}

#define SETUP_TIME_SEC 600UL

/**
 * @brief Возвращает параметры считанные с Waterius`a для отображения данных в реальном времени
 *
 * @return int
 */
void onGetStates(Portal *portal, AsyncWebServerRequest *request)
{
    if(portal->captivePortal(request))
        return;
    LOG_INFO(F("Portal onGetState GET ") << request->host() << request->url());
    SlaveData runtime_data;
    JsonConstructor json(1024);
    json.begin();
    if (masterI2C.getSlaveData(runtime_data))
    {
        if (runtime_data.impulses0 > data.impulses0)
        {
            json.push(F("state0good"), F("Подключён"));
            json.push(F("state0bad"), F(""));
        }
        else
        {
            json.push(F("state0good"), F(""));
            json.push(F("state0bad"), F("Подключён"));
        }
        if (runtime_data.impulses1 > data.impulses1)
        {
            json.push(F("state1good"), F("Подключён"));
            json.push(F("state1bad"), F(""));
        }
        else
        {
            json.push(F("state1good"), F(""));
            json.push(F("state1bad"), F("Подключён"));
        }
        json.push(F("elapsed"), (uint32_t)(SETUP_TIME_SEC - millis() / 1000.0));
        json.push(F("factor_cold_feedback"), get_auto_factor(runtime_data.impulses1, data.impulses1));
        json.push(F("factor_hot_feedback"), get_auto_factor(runtime_data.impulses0, data.impulses0));
        bool _fail=false;
        if (_fail)
        {
            json.push(F("fail"), F("1"));
        }
        else
        {
            json.push(F("fail"), F(""));
        }
        json.push(F("error"), F(""));
    }
    else
    {
        json.push(F("error"), F("Ошибка связи с МК"));
        json.push(F("factor_cold_feedback"), 1);
        json.push(F("factor_hot_feedback"), 1);
    }
    json.end();
    request->send(200, F("application/json"), json.c_str());
    LOG_INFO(json.c_str());
}


/**
 * @brief Возвращает конфигурацию прибора в формате JSON для заполнения полей настройки
 *
 * @return
 */
void onGetConfig(Portal *portal, AsyncWebServerRequest *request)
{
    if(portal->captivePortal(request))
        return;
    LOG_INFO(F("Portal onGetConfig GET ") << request->host() << request->url());
    JsonConstructor json(2048);
    json.begin();
    json.push(F("wmail"), sett.waterius_email);
    json.push(F("whost"), sett.waterius_host);
    json.push(F("mperiod"), sett.wakeup_per_min);
    json.push(F("s"), WiFi.SSID().c_str());
    json.push(F("p"), WiFi.psk().c_str());
    json.push(F("bhost"), sett.blynk_host);
    json.push(F("bkey"), sett.blynk_key);
    json.push(F("bemail"), sett.blynk_email);
    json.push(F("btitle"), sett.blynk_email_title);
    json.push(F("btemplate"), sett.blynk_email_template);
    json.push(F("mhost"), sett.mqtt_host);
    json.push(F("mport"), sett.mqtt_port);
    json.push(F("mlogin"), sett.mqtt_login);
    json.push(F("mpassword"), sett.mqtt_password);
    json.push(F("mtopic"), sett.mqtt_topic);
    json.push(F("auto_discovery_checkbox"), sett.mqtt_auto_discovery);
    json.push(F("discovery_topic"), sett.mqtt_discovery_topic);
    json.push(F("mac"), WiFi.macAddress().c_str());
    json.push(F("ip"), Portal::ipToString(sett.ip).c_str());
    json.push(F("sn"), Portal::ipToString(sett.mask).c_str());
    json.push(F("gw"), Portal::ipToString(sett.gateway).c_str());
    json.push(F("ntp"), sett.ntp_server);
    json.push(F("factorCold"), sett.factor1);
    json.push(F("factorHot"), sett.factor0);
    json.push(F("serialCold"), sett.serial0);
    json.push(F("serialHot"), sett.serial1);
    json.push(F("ch0"), cdata.channel0, 3);
    json.push(F("ch1"), cdata.channel1, 3);
    json.end();
    AsyncResponseStream *response = request->beginResponseStream(F("application/json"));
    response->addHeader("Server", "ESP Async Web Server");
    response->print(json.c_str());
    request->send(response);
}

void onErase(AsyncWebServerRequest *request)
{
    LOG_INFO(F("Portal onErase GET ") << request->host()<< request->url());
    LOG_INFO(F("ESP erase config"));
    ESP.eraseConfig();
    delay(100);
    LOG_INFO(F("EEPROM erase"));
    ESP.flashEraseSector(((EEPROM_start - 0x40200000) / SPI_FLASH_SEC_SIZE));
    delay(1000);
    ESP.reset();
}


/**
 * @brief Обработчик POST запроса с новыми параметрами настройки прибора
 *
 * @return int
 */
void onPostWifiSave(AsyncWebServerRequest *request)
{
    LOG_INFO(F("Portal onPostWiFiSave POST ") << request->host()<< request->url());
    Portal::UpdateParamStr(request, "s", sett.wifi_ssid, WIFI_SSID_LEN - 1);
    Portal::UpdateParamStr(request, "p", sett.wifi_password, WIFI_PWD_LEN - 1);
    if (Portal::UpdateParamStr(request, PARAM_WMAIL, sett.waterius_email, EMAIL_LEN))
    {
        generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, sett.waterius_email);
    }
    Portal::SetParamStr(request, PARAM_WHOST, sett.waterius_host, HOST_LEN - 1);
    Portal::SetParamUInt(request, PARAM_MPERIOD, &sett.wakeup_per_min);

    Portal::SetParamStr(request, PARAM_BHOST, sett.blynk_host, HOST_LEN - 1);
    Portal::SetParamStr(request, PARAM_BKEY, sett.blynk_key, BLYNK_KEY_LEN);
    Portal::SetParamStr(request, PARAM_BMAIL, sett.blynk_email, EMAIL_LEN);
    Portal::SetParamStr(request, PARAM_BTITLE, sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN);
    Portal::SetParamStr(request, PARAM_BTEMPLATE, sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN);

    Portal::SetParamStr(request, PARAM_MHOST, sett.mqtt_host, HOST_LEN - 1);
    Portal::SetParamUInt(request, PARAM_MPORT, &sett.mqtt_port);
    Portal::SetParamStr(request, PARAM_MLOGIN, sett.mqtt_login, MQTT_LOGIN_LEN);
    Portal::SetParamStr(request, PARAM_MPASSWORD, sett.mqtt_password, MQTT_PASSWORD_LEN);
    Portal::SetParamStr(request, PARAM_MTOPIC, sett.mqtt_topic, MQTT_TOPIC_LEN);
    Portal::SetParamByte(request, PARAM_MDAUTO, &sett.mqtt_auto_discovery);
    Portal::SetParamStr(request, PARAM_MDTOPIC, sett.mqtt_discovery_topic, MQTT_TOPIC_LEN);

    Portal::SetParamIP(request, PARAM_IP, &sett.ip);
    Portal::SetParamIP(request, PARAM_GW, &sett.gateway);
    Portal::SetParamIP(request, PARAM_SN, &sett.mask);
    Portal::SetParamStr(request, PARAM_NTP, sett.ntp_server, HOST_LEN);

    uint8_t combobox_factor = -1;
    if (Portal::SetParamByte(request, PARAM_FACTORCOLD, &combobox_factor))
    {
        sett.factor1 = get_factor(combobox_factor, data.impulses1, data.impulses1, 1);
        LOG_INFO("cold dropdown=" << combobox_factor);
        LOG_INFO("factorCold=" << sett.factor1);
    }
    if (Portal::SetParamByte(request, PARAM_FACTORHOT, &combobox_factor))
    {
        sett.factor0 = get_factor(combobox_factor, data.impulses1, data.impulses1, 1);
        LOG_INFO("hot dropdown=" << combobox_factor);
        LOG_INFO("factorHot=" << sett.factor0);
    }
    Portal::SetParamStr(request, PARAM_SERIALCOLD, sett.serial1, SERIAL_LEN);
    Portal::SetParamStr(request, PARAM_SERIALHOT, sett.serial0, SERIAL_LEN);

    if (Portal::SetParamFloat(request, PARAM_CH0, &sett.channel0_start))
    {
        sett.impulses0_start = data.impulses0;
        sett.impulses0_previous = sett.impulses0_start;
        LOG_INFO("impulses0=" << sett.impulses0_start);
    }
    if (Portal::SetParamFloat(request, PARAM_CH1, &sett.channel1_start))
    {
        sett.impulses1_start = data.impulses1;
        sett.impulses1_previous = sett.impulses1_start;
        LOG_INFO("impulses1=" << sett.impulses1_start);
    }

    WiFi.persistent(true);
    bool fail = WiFi.begin(sett.wifi_ssid, sett.wifi_password);
    WiFi.persistent(false);
    // Запоминаем кол-во импульсов Attiny соответствующих текущим показаниям счетчиков
    if (fail)
    {
        sett.setup_time = millis();
        sett.setup_finished_counter++;

        store_config(sett);
        AsyncWebServerResponse *response = request->beginResponse(200, "", F("Save configuration - Successfully."));
        response->addHeader("Refresh", "2; url=/exit");
        request->send(response);
    }
    else
    {
        request->redirect("/");
    }
}

void loop()
{
    uint8_t mode = SETUP_MODE; // TRANSMIT_MODE;
    bool config_loaded = false;

    // спрашиваем у Attiny85 повод пробуждения и данные
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

            WiFi.persistent(false);
            WiFi.disconnect();

            wifi_set_mode(WIFI_AP);
            String apName = get_ap_name();
            WiFi.softAP(apName.c_str());


            // setup_ap(sett, data, cdata);
            DNSServer *dns = new DNSServer();
            dns->start(53, "*", WiFi.softAPIP());
            // запускаем сервер
            Portal *portal = new Portal();
            portal->on("/states", HTTP_GET, std::bind(onGetStates, portal, std::placeholders::_1));
            portal->on("/config", HTTP_GET, std::bind(onGetConfig, portal, std::placeholders::_1));
            portal->on("/erase", HTTP_GET, onErase);
            portal->on("/wifisave", HTTP_POST, onPostWifiSave);
            portal->begin();

            WiFi.scanNetworks(true);
            while (!portal->doneettings())
            {
                dns->processNextRequest();
                yield();
            }
            portal->end();
            dns->stop();
            delete portal;
            delete dns;
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

                JsonConstructor json(JSON_DYNAMIC_MSG_BUFFER);

                // Подключаемся и подписываемся на мктт
                if (is_mqtt(sett))
                {
                    connect_and_subscribe_mqtt(sett);
                    LOG_INFO(F("MQTT: while(){ loop();}"));
                    uint32_t t = millis() + 100;
                    uint32_t c = 0;
                    while ((millis() < t) && (!periodUpdated))
                    {
                        mqtt_client.loop();
                        c++;
                        delay(2);
                    }
                    LOG_INFO(F("MQTT: loop count ") << c);
                }

                // устанавливать время только при использовани хттпс или мктт
                if (is_mqtt(sett) || is_https(sett.waterius_host))
                {
                    sync_ntp_time();
                }

                voltage_ticker.detach(); // перестаем обновлять перед созданием объекта с данными
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

                // Формироуем JSON
                get_json_data(sett, data, cdata, json);

                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

#ifndef HTTPS_DISABLED
                if (send_http(sett, json.c_str()))
                {
                    LOG_INFO(F("HTTP: Send OK"));
                }
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
#endif

#ifndef BLYNK_DISABLED
                if (send_blynk(sett, data, cdata))
                {
                    LOG_INFO(F("BLYNK: Send OK"));
                }
                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());
#endif

#ifndef MQTT_DISABLED
                if (is_mqtt(sett))
                {
                    if (send_mqtt(sett, data, json.c_str()))
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

                update_config(sett, data, cdata);

                if (!masterI2C.setWakeUpPeriod(sett.set_wakeup))
                {
                    LOG_ERROR(F("Wakeup period wasn't set"));
                }
                else //"Разбуди меня через..."
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

    if (!config_loaded) {
        delay(500);
        blink_led(3, 1000, 500);
    }

    masterI2C.sendCmd('Z'); // "Можешь идти спать, attiny"

    ESP.deepSleepInstant(0, RF_DEFAULT); // Спим до следущего включения EN. Instant не ждет 92мс


}