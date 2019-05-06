

#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>

#include "wifi_settings.h"
#include "master_i2c.h"
#include "setup_ap.h"
#include "sender_blynk.h"
#include "sender_mqtt.h"
#include "UserClass.h"
#include "utils.h"
#include "cert.h"

MasterI2C masterI2C; // Для общения с Attiny85 по i2c

SlaveData data; // Данные от Attiny85
Settings sett;  // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; //вычисляемые данные

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
void calculate_values(Settings &sett, SlaveData &data, CalculatedData *cdata)
{

    LOG_NOTICE("ESP", "new impulses=" << data.impulses0 << " " << data.impulses1);

    if (sett.liters_per_impuls > 0) {
        cdata->channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.liters_per_impuls;
        cdata->channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.liters_per_impuls;
        LOG_NOTICE("ESP", "new values=" << cdata->channel0 << " " << cdata->channel1);
        cdata->delta0  = (data.impulses0 - sett.impulses0_previous)*sett.liters_per_impuls;
        cdata->delta1 = (data.impulses1 - sett.impulses1_previous)*sett.liters_per_impuls;
        LOG_NOTICE("ESP", "delta values=" << cdata->delta0 << " " << cdata->delta1);
    }
}


void loop()
{
    memset(&cdata, 0, sizeof(cdata));
    uint8_t mode = TRANSMIT_MODE;

	// спрашиваем у Attiny85 повод пробуждения и данные
    if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data)) {
        if (mode == SETUP_MODE) {
            //Режим настройки - запускаем точку доступа на 192.168.4.1

            //Загружаем конфигурацию из EEPROM
            loadConfig(sett);

            //Вычисляем текущие показания
            calculate_values(sett, data, &cdata);

            //Запускаем точку доступа с вебсервером
            setup_ap(sett, data, cdata);
        }
        else {   
            // Режим передачи новых показаний
            if (!loadConfig(sett)) {
                LOG_ERROR("ESP", "error loading config");
            }
            else {
                //Вычисляем текущие показания
                calculate_values(sett, data, &cdata);

                LOG_NOTICE("WIF", "Starting");
                
                WiFi.mode(WIFI_STA);
                //WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                WiFi.begin(); 

                //Ожидаем подключения к точке доступа
                uint32_t start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {
                    LOG_NOTICE("WIF", "Status: " << WiFi.status());
                    delay(200);
                }

                if (WiFi.status() == WL_CONNECTED) {

                    LOG_NOTICE("WIF", "Connected, IP: " << WiFi.localIP().toString());

#ifdef SEND_BLYNK
                    if (send_blynk(sett, data, cdata)) {
                        LOG_NOTICE("BLK", "send ok");
                    }
#endif  

#ifdef SEND_MQTT
                    if (send_mqtt(sett, data, cdata)) {
                        LOG_NOTICE("MQT", "send ok");
                    }
#endif  

#ifdef SEND_WATERIUS
                    UserClass::sendNewData(sett, data, cdata);
#endif
                }

                //Сохраним текущие значения в памяти.
                sett.impulses0_previous = data.impulses0;
                sett.impulses1_previous = data.impulses1;
                storeConfig(sett);
            }
        }
    }

    LOG_NOTICE("ESP", "Going to sleep");
    masterI2C.sendCmd('Z');        // "Можешь идти спать, attiny"
    LOG_END();
    
    twi_stop();
    ESP.deepSleep(0, RF_DEFAULT);  // Спим до следущего включения EN
}
