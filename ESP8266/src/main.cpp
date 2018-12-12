
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>

#include "wifi_settings.h"
#include "master_i2c.h"
#include "setup_ap.h"
#include "sender_blynk.h"
#include "sender_tcp.h"
#include "sender_json.h"

MasterI2C masterI2C; // Для общения с Attiny85 по i2c

SlaveData data; // Данные от Attiny85
Settings sett;  // Настройки соединения и предыдущие показания из EEPROM

/*
Выполняется однократно при включении
*/
void setup()
{
    memset(&data, 0, sizeof(data)); // На всякий случай
    LOG_BEGIN(115200);    //Включаем логгирование на пине TX, 115200 8N1
    LOG_NOTICE("ESP", "Booted");
    masterI2C.begin();    //Включаем i2c master
}

/*
Берем начальные показания и кол-во импульсов, 
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(Settings &sett, SlaveData &data, float *channel0, float *channel1)
{

    LOG_NOTICE("ESP", "new impulses=" << data.impulses0 << " " << data.impulses1);

    if (sett.liters_per_impuls > 0) {
        *channel0 = sett.channel0_start + (data.impulses0 - sett.impules0_start) / 1000.0 * sett.liters_per_impuls;
        *channel1 = sett.channel1_start + (data.impulses1 - sett.impules1_start) / 1000.0 * sett.liters_per_impuls;
        LOG_NOTICE("ESP", "new values=" << *channel0 << " " << *channel1);
    }
}

void loop()
{
    float channel0, channel1;
    uint8_t mode;

	// спрашиваем у Attiny85 повод пробуждения и данные
    if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data)) {
        if (mode == SETUP_MODE) {

            // Режим настройки, запускаем точку доступа на 192.168.4.1
            loadConfig(sett);

            //Вычисляем текущие показания
            calculate_values(sett, data, &channel0, &channel1);

            //Запускаем точку доступа с вебсервером
            setup_ap(sett, data, channel0, channel1);
        }
        else {   
            // Режим передачи новых показаний
            if (!loadConfig(sett)) {
                LOG_ERROR("ESP", "error loading config");
            }
            else {
                //Вычисляем текущие показания
                calculate_values(sett, data, &channel0, &channel1);

                LOG_NOTICE("WIF", "Starting Wifi");
                //WiFi.mode(WIFI_STA);
                
                //WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                WiFi.begin(); 

                //Ожидаем подключения к точке доступа
                uint32_t start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {

                    LOG_NOTICE("WIF", "Wifi status: " << WiFi.status());
                    delay(200);
                }

                if (WiFi.status() == WL_CONNECTED) {

                    LOG_NOTICE("WIF", "Connected, got IP address: " << WiFi.localIP().toString());

#ifdef SEND_BLYNK
                    if (send_blynk(sett, data, channel0, channel1)) {
                        LOG_NOTICE("BLK", "send ok");
                    }
#endif
#ifdef SEND_JSON
                    if (send_json(sett, data, channel0, channel1)) {
                        LOG_NOTICE("JSN", "send ok");
                    }
#endif
#ifdef SEND_TCP
                    if (send_tcp(sett, data, channel0, channel1)) {
                        LOG_NOTICE("TCP", "send ok");
                    }
#endif
                    //Сохраним текущие значения в памяти.
                    sett.channel0_previous = channel0;
                    sett.channel1_previous = channel1;
                    storeConfig(sett);
                }
            }
        }
    }

    LOG_NOTICE("ESP", "Going to sleep");
    LOG_END();
    masterI2C.sendCmd('Z');        // "Можешь идти спать, attiny"
    ESP.deepSleep(0, RF_DEFAULT);  // Спим до следущего включения EN
}
