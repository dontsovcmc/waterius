
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>

#include "wifi_settings.h"
#include "master_i2c.h"
#include "setup_ap.h"
#include "sender_blynk.h"
#include "sender_mqtt.h"
#include "UserClass.h"
#include "voltage.h"
#include "utils.h"
#include "cert.h"

MasterI2C masterI2C; // Для общения с Attiny85 по i2c

SlaveData data; // Данные от Attiny85
Settings sett;  // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; //вычисляемые данные
Voltage volt; // клас монитора питания

ADC_MODE(ADC_VCC);

/*
Выполняется однократно при включении
*/
void setup()
{
    WiFi.mode(WIFI_OFF);  //TODO  а нужна ли?

    memset(&cdata, 0, sizeof(cdata));
    memset(&data, 0, sizeof(data));
    LOG_BEGIN(115200);    //Включаем логгирование на пине TX, 115200 8N1
    LOG_INFO(FPSTR(S_ESP), F("Booted"));
    masterI2C.begin();    //Включаем i2c master
    volt.begin();
}

/*
Берем начальные показания и кол-во импульсов, 
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    LOG_INFO(FPSTR(S_ESP), F("new impulses=") << data.impulses0 << " " << data.impulses1);

    if ((sett.factor1 > 0) && (sett.factor0 > 0)) {
        cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.factor0;
        cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.factor1;
        LOG_INFO(FPSTR(S_ESP), F("new value0=") << cdata.channel0 << F(" value1=") << cdata.channel1);
        
        cdata.delta0  = (data.impulses0 - sett.impulses0_previous) * sett.factor0;
        cdata.delta1 = (data.impulses1 - sett.impulses1_previous) * sett.factor1;
        LOG_INFO(FPSTR(S_ESP), F("delta0=") << cdata.delta0 << F(" delta1=") << cdata.delta1);
    }
}


void loop()
{
    uint8_t mode = SETUP_MODE;//TRANSMIT_MODE;
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

                    if(!masterI2C.setWakeUpPeriod(sett.wakeup_per_min)){
                        LOG_ERROR(FPSTR(S_I2C), F("Wakeup period wasn't set"));
                    } //"Разбуди меня через..."
                    else{
                        LOG_INFO(FPSTR(S_I2C), F("Wakeup period, min:") << sett.wakeup_per_min);
                    }

                    //WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                    WiFi.begin(); 

                    //Ожидаем подключения к точке доступа
                    uint32_t start = millis();
                    while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {
                        LOG_INFO(FPSTR(S_WIF), F("Status: ") << WiFi.status());
                        delay(300);
                    
                        volt.get();
                        //В будущем добавим success, означающее, что напряжение не критично изменяется, можно продолжать
                        //иначе есть риск ошибки ESP и стирания конфигурации
                    }
                    cdata.voltage_diff=volt.diff(); 
                    cdata.low_voltage = volt.low_voltage();
                }
            }
            
            if (success 
                && WiFi.status() == WL_CONNECTED
                && masterI2C.getSlaveData(data)) {  //т.к. в check_voltage не проверяем crc
                data.voltage=volt.value();

                print_wifi_mode();
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
    
    ESP.deepSleepInstant(0, RF_DEFAULT);  // Спим до следущего включения EN. Instant не ждет 92мс
}
