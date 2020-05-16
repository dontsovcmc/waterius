
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
    WiFi.mode(WIFI_OFF);
    memset(&cdata, 0, sizeof(cdata));
    memset(&data, 0, sizeof(data)); // На всякий случай
    LOG_BEGIN(115200);    //Включаем логгирование на пине TX, 115200 8N1
    LOG_INFO(FPSTR(S_ESP), F("Booted"));
    masterI2C.begin();    //Включаем i2c master
}

/*
Берем начальные показания и кол-во импульсов, 
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    LOG_INFO(FPSTR(S_ESP), F("new impulses=") << data.impulses0 << " " << data.impulses1);

    if (sett.liters_per_impuls > 0) {
        cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.liters_per_impuls;
        cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.liters_per_impuls;
        LOG_INFO(FPSTR(S_ESP), F("new value0=") << cdata.channel0 << F(" value1=") << cdata.channel1);
        
        cdata.delta0  = (data.impulses0 - sett.impulses0_previous)*sett.liters_per_impuls;
        cdata.delta1 = (data.impulses1 - sett.impulses1_previous)*sett.liters_per_impuls;
        LOG_INFO(FPSTR(S_ESP), F("delta0=") << cdata.delta0 << F(" delta1=") << cdata.delta1);
    }
}

#define LOW_BATTERY_DIFF_MV 50  //надо еще учесть качество замеров (компаратора у attiny)
#define ALERT_POWER_DIFF_MV 100

bool check_voltage(SlaveData &data, CalculatedData &cdata)
{   
    uint32_t prev = data.voltage;
	if (masterI2C.getSlaveData(data)) {
        uint32_t diff = abs(prev - data.voltage);
        if (diff > cdata.voltage_diff) {
            cdata.voltage_diff = diff;
        }
        
        cdata.low_voltage = cdata.voltage_diff >= LOW_BATTERY_DIFF_MV;
        return cdata.voltage_diff < ALERT_POWER_DIFF_MV;
	}
    return true; //пропустим если ошибка i2c
}

void loop()
{
    uint8_t mode = TRANSMIT_MODE;

	// спрашиваем у Attiny85 повод пробуждения и данные
    if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data)) {
        //Загружаем конфигурацию из EEPROM
        bool success = loadConfig(sett);
        if (!success) {
            LOG_ERROR(FPSTR(S_ESP), F("Error loading config"));
        }

        //Вычисляем текущие показания
        calculate_values(sett, data, cdata);

        if (mode == SETUP_MODE) { //Режим настройки - запускаем точку доступа на 192.168.4.1
            //Запускаем точку доступа с вебсервером
            WiFi.mode(WIFI_AP_STA);
            setup_ap(sett, data, cdata);
            
            //Необходимо протестировать WiFi, чтобы роутер не блокировал повторное подключение.
            //
            //masterI2C.sendCmd('T'); //Передаем в Attiny, что режим "Передача". 
            //                        //ESP перезагрузится и передаст данные 
            //LOG_END();
            //WiFi.forceSleepBegin();
            //delay(100);
            //ESP.restart();

            success = false;
        }
        
        if (success) {
            if (mode == TRANSMIT_MODE) { 
                //Проснулись для передачи показаний
                LOG_INFO(FPSTR(S_WIF), F("Starting Wi-fi"));
                
                if (sett.ip != 0) {
                    success = WiFi.config(sett.ip, sett.gateway, sett.mask, sett.gateway, IPAddress(8,8,8,8));
                    if (success) {
                        LOG_INFO(FPSTR(S_WIF), F("Static IP OK"));
                    } else {
                        LOG_ERROR(FPSTR(S_WIF), F("Static IP FAILED"));
                    }
                }

                if (success) {
                    //WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                    WiFi.begin(); 

                    //Ожидаем подключения к точке доступа
                    uint32_t start = millis();
                    while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {
                        LOG_INFO(FPSTR(S_WIF), F("Status: ") << WiFi.status());
                        delay(300);

                        check_voltage(data, cdata);
                        //В будущем добавим success, означающее, что напряжение не критично изменяется, можно продолжать
                        //иначе есть риск ошибки ESP и стирания конфигурации
                    }
                }
            }
            
            if (success 
                && WiFi.status() == WL_CONNECTED) {

                LOG_INFO(FPSTR(S_WIF), F("Connected, IP: ") << WiFi.localIP().toString());
                
                cdata.rssi = WiFi.RSSI();
                LOG_DEBUG(FPSTR(S_WIF), F("RSSI: ") << cdata.rssi);

                if (send_blynk(sett, data, cdata)) {
                    LOG_INFO(FPSTR(S_BLK), F("Send OK"));
                }
                
                if (send_mqtt(sett, data, cdata)) {
                    LOG_INFO(FPSTR(S_MQT), F("Send OK"));
                }

                UserClass::sendNewData(sett, data, cdata);

                //Сохраним текущие значения в памяти.
                sett.impulses0_previous = data.impulses0;
                sett.impulses1_previous = data.impulses1;
                //Перешлем время на сервер при след. включении
                sett.wake_time = millis();

                storeConfig(sett);
            }
        } 
    }

    LOG_INFO(FPSTR(S_ESP), F("Going to sleep"));
    
    masterI2C.sendCmd('Z');        // "Можешь идти спать, attiny"
    LOG_END();
    
    ESP.deepSleep(0, RF_DEFAULT);  // Спим до следущего включения EN
}
