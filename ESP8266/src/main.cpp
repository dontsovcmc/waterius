#include <Arduino.h>
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>

#include "portal.h"

#include "wifi_settings.h"
#include "master_i2c.h"
//#include "setup_ap.h"
#include "sender_blynk.h"
#include "sender_mqtt.h"
#include "UserClass.h"
#include "voltage.h"
#include "utils.h"
#include "cert.h"
#include "WateriusHttps.h"
#include "setup.h"
#include <ArduinoJson.h>

MasterI2C masterI2C; // Для общения с Attiny85 по i2c

SlaveData data;       // Данные от Attiny85
Settings sett;        // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; //вычисляемые данные
Voltage voltage;      // клас монитора питания
ADC_MODE(ADC_VCC);

/* Web server*/
// Portal portal;

#define AP_NAME "Waterius_" FIRMWARE_VERSION

/*
Выполняется однократно при включении
*/
void setup()
{
    set_hostname();
    
    memset(&cdata, 0, sizeof(cdata));
    memset(&data, 0, sizeof(data));
    LOG_BEGIN(115200); //Включаем логгирование на пине TX, 115200 8N1
    LOG_INFO(F("Booted"));

    LOG_INFO(F("Saved SSID: ") << WiFi.SSID());
    LOG_INFO(F("Saved password: ") << WiFi.psk());

    masterI2C.begin(); //Включаем i2c master
    voltage.begin();
    LittleFS.begin();

    struct station_config conf_compare;
    wifi_station_get_config_default(&conf_compare);
    LOG_INFO((char*)&conf_compare.ssid);
    LOG_INFO((char*)&conf_compare.password);
    LOG_INFO(conf_compare.bssid_set);
    //LOG_INFO(conf_compare.bssid);
    LOG_INFO(conf_compare.threshold.rssi);
    LOG_INFO(conf_compare.threshold.authmode);
}

void wifi_handle_event_cb(System_Event_t *evt)
{

    switch (evt->event)
    {
    case EVENT_STAMODE_CONNECTED:
        cdata.channel = evt->event_info.connected.channel;
        cdata.router_mac = evt->event_info.connected.bssid[0];
        cdata.router_mac = cdata.router_mac << 8;
        cdata.router_mac |= evt->event_info.connected.bssid[1];
        cdata.router_mac = cdata.router_mac << 8;
        cdata.router_mac |= evt->event_info.connected.bssid[2];
        break;
    }
}

/*
Берем начальные показания и кол-во импульсов,
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
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

void FirstConnect()
{
    LOG_INFO(F("No (or wrong) saved WiFi credentials. Doing a fresh connect."));
    if (!WiFi.getAutoConnect())
        WiFi.setAutoConnect(true);
    if (!WiFi.getPersistent())
        WiFi.persistent(true);
    WiFi.begin();
    uint8_t counter = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        if (++counter > 20)
        { // allow up to 10-sec to connect to wifi
            LOG_INFO(F("wifi timed-out. Rebooting.."));
            delay(10); // so the serial message has time to get sent
            // ESP.restart();
        }
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        LOG_INFO(F("Reconnect"));
        char ssid[33];
        char psk[65];
        int32_t channel;
        uint8 home_mac[6];
        strcpy(ssid, WiFi.SSID().c_str());
        strcpy(psk, WiFi.psk().c_str());
        channel = WiFi.channel();
        memcpy(home_mac, WiFi.BSSID(), 6);
       // LOG_INFO("SSID=" << home_mac << " PSK=" << psk << " channel=" << channel << " BSSID=" << home_mac);
        WiFi.begin(ssid, psk, channel, home_mac, true);
        counter = 0;
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            if (++counter > 20)
            { 
                LOG_INFO(F("Reconnect OK."));
                delay(10);
            }
        }
        LOG_INFO(F("WiFi connected and credentials saved"));
    }
}

    void loop()
    {
        uint8_t mode = SETUP_MODE; // TRANSMIT_MODE;

        // спрашиваем у Attiny85 повод пробуждения и данные
        if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data))
        {
            //Загружаем конфигурацию из EEPROM
            bool success = loadConfig(sett);
            if (!success)
            {
                LOG_ERROR(F("Error loading config"));
            }

            sett.mode = mode;

            //Вычисляем текущие показания
            calculate_values(sett, data, cdata);

            if (mode == SETUP_MODE)
            {
                // Режим настройки - запускаем точку доступа на 192.168.4.1
                // Запускаем точку доступа с вебсервером
                WiFi.mode(WIFI_AP);
                // setup_ap(sett, data, cdata);

                // list litlefs files
                File root = LittleFS.open("/", "r");

                File file = root.openNextFile();

                while (file)
                {
                    LOG_INFO(F("/") << file.name());
                    file = root.openNextFile();
                }

                // настраиваем точку доступа
                LOG_INFO(F("Start ConfigPortal"));
                // Запуск веб сервера на 192.168.4.1

                WiFi.softAP(AP_NAME);

                // Успешно подключились к Wi-Fi, можно засыпать
                LOG_INFO(F("Connected to wifi. Save settings, go to sleep"));

                //запускаем сервер
                Portal portal = Portal();
                portal.begin();

                WiFi.scanNetworks(true);
                while (!portal.doneettings())
                {
                    yield();
                };
                portal.end();
                portal.~Portal();

                // ухищрения, чтобы не стереть SSID, pwd
                if (WiFi.getPersistent() == true)
                    WiFi.persistent(false); // disable saving wifi config into SDK flash area
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);   // отключаем WIFI
                WiFi.persistent(true); // enable saving wifi config into SDK flash area

                LOG_INFO(F("Set mode MANUAL_TRANSMIT to attiny"));
                masterI2C.sendCmd('T'); // Режим "Передача"

                LOG_INFO(F("Restart ESP"));
                LOG_END();
                WiFi.forceSleepBegin();
                delay(1000);
                ESP.restart();

                // never happend here
                success = false;
            }
            if (success)
            {
                if (mode != SETUP_MODE)
                {
                    //Проснулись для передачи показаний
                    LOG_INFO(F("Starting Wi-fi"));

                    wifi_set_event_handler_cb(wifi_handle_event_cb);

                    if (sett.ip != 0)
                    {
                        success = WiFi.config(sett.ip, sett.gateway, sett.mask, sett.gateway, IPAddress(8, 8, 8, 8));
                        if (success)
                        {
                            LOG_INFO(F("Static IP OK"));
                        }
                        else
                        {
                            LOG_ERROR(F("Static IP FAILED"));
                        }
                    }

                    if (success)
                    {
                        //WiFi.mode(WIFI_STA); //без этого не записывается hostname
                        //set_hostname();
                        uint32_t update = 0;
                        while (WiFi.status() != WL_CONNECTED)
                        {
                            delay(5); // use small delays, NOT 500ms
                            if(update<millis()){
                                update = millis()+300;
                                LOG_INFO(F("Status: ") << WiFi.status());
                                voltage.update();
                            }
                            if (millis() > 5000)
                                break; // 5 sec timeout
                        }
                        if (millis() > 5000)
                            FirstConnect();
                        // WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                        // WiFi.begin();

                        //Ожидаем подключения к точке доступа
                        uint32_t start = millis();
                        while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT)
                        {
                            LOG_INFO(F("Status: ") << WiFi.status());
                            delay(300);

                            voltage.update();
                            //В будущем добавим success, означающее, что напряжение не критично изменяется, можно продолжать
                            //иначе есть риск ошибки ESP и стирания конфигурации
                        }
                        cdata.voltage = voltage.value();
                        cdata.voltage_diff = voltage.diff();
                        cdata.low_voltage = voltage.low_voltage();
                    }
                }

                if (success && WiFi.status() == WL_CONNECTED)
                {

                    print_wifi_mode();
                    LOG_INFO(F("Connected, IP: ") << WiFi.localIP().toString());

                    cdata.rssi = WiFi.RSSI();
                    LOG_INFO(F("RSSI: ") << cdata.rssi);
                    LOG_INFO(F("channel: ") << cdata.channel);
                    LOG_INFO(F("MAC: ") << String(cdata.router_mac, HEX));

                    if (send_blynk(sett, data, cdata))
                    {
                        LOG_INFO(F("Send OK"));
                    }

                    if (send_mqtt(sett, data, cdata))
                    {
                        LOG_INFO(F("Send OK"));
                    }

                    UserClass::sendNewData(sett, data, cdata);

                    //Сохраним текущие значения в памяти.
                    sett.impulses0_previous = data.impulses0;
                    sett.impulses1_previous = data.impulses1;

                    //Перешлем время на сервер при след. включении
                    sett.wake_time = millis();

                    //Перерасчет времени пробуждения
                    if (mode == TRANSMIT_MODE)
                    {
                        time_t now = time(nullptr);
                        time_t t1 = (now - sett.lastsend) / 60;
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
                    sett.lastsend = time(nullptr);

                    if (!masterI2C.setWakeUpPeriod(sett.set_wakeup))
                    {
                        LOG_ERROR(F("Wakeup period wasn't set"));
                    } //"Разбуди меня через..."
                    else
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