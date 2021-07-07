
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

/*
Выполняется однократно при включении
*/
void setup()
{
    WiFi.mode(WIFI_OFF);  //TODO  а нужна ли?

    memset(&cdata, 0, sizeof(cdata));
    memset(&data, 0, sizeof(data));
    LOG_BEGIN(115200);    //Включаем логгирование на пине TX, 115200 8N1
    //LOG_INFO(FPSTR(S_ESP), F("Booted"));
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_ESP)); LOG(F("BOOT"));
    #endif
    masterI2C.begin();    //Включаем i2c master
}

/*
Берем начальные показания и кол-во импульсов, 
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_ESP)); LOG(F("new impulses=")); LOG(data.impulses0); LOG(F(" ")); LOG(data.impulses1);
    #endif

    if ((sett.factor1 > 0) && (sett.factor0 > 0)) {
        cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.factor0;
        cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.factor1;
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_ESP)); LOG(F("new value0=")); LOG(cdata.channel0); LOG(F(" value1=")); LOG(cdata.channel1);
        #endif
        
        cdata.delta0  = (data.impulses0 - sett.impulses0_previous) * sett.factor0;
        cdata.delta1 = (data.impulses1 - sett.impulses1_previous) * sett.factor1;
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_ESP)); LOG(F("delta0=")); LOG(cdata.delta0); LOG(F(" delta1=")); LOG(cdata.delta1);
        #endif
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
            //LOG_ERROR(FPSTR(S_ESP), F("Error loading config"));
            LOG_START(FPSTR(S_ERROR),FPSTR(S_ESP)); LOG(F("Error loading config"));
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
                #if LOGLEVEL>=1
                LOG_START(FPSTR(S_INFO) ,FPSTR(S_WIF)); LOG(F("Starting Wi-fi"));
                #endif
                
                if (sett.ip != 0) {
                    success = WiFi.config(sett.ip, sett.gateway, sett.mask, sett.gateway, IPAddress(8,8,8,8));
                    if (success) {
                        #if LOGLEVEL>=1
                        LOG_START(FPSTR(S_INFO) ,FPSTR(S_WIF)); LOG(F("Static IP OK"));
                        #endif
                    } else {
                        #if LOGLEVEL>=0
                        LOG_START(FPSTR(S_ERROR) ,FPSTR(S_WIF)); LOG(F("Static IP FAILED"));
                        #endif
                    }
                }

                if (success) {

                    if(!masterI2C.setWakeUpPeriod(sett.wakeup_per_min)){
                        #if LOGLEVEL>=0
                        LOG_START(FPSTR(S_ERROR) ,FPSTR(S_I2C)); LOG(F("Wakeup period wasn't set"));
                        #endif
                    } //"Разбуди меня через..."
                    else{
                        #if LOGLEVEL>=1
                        LOG_START(FPSTR(S_INFO), FPSTR(S_I2C)); LOG("Wakeup period, min:"); LOG(sett.wakeup_per_min);
                        #endif
                    }

                    //WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                    WiFi.begin(); 

                    //Ожидаем подключения к точке доступа
                    uint32_t start = millis();
                    while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {
                        #if LOGLEVEL>=1
                        LOG_START(FPSTR(S_INFO) ,FPSTR(S_WIF)); LOG(F("Status: ")); LOG(WiFi.status());
                        #endif
                        delay(300);
                    
                        check_voltage(data, cdata);
                        //В будущем добавим success, означающее, что напряжение не критично изменяется, можно продолжать
                        //иначе есть риск ошибки ESP и стирания конфигурации
                    }
                }
            }
            
            if (success 
                && WiFi.status() == WL_CONNECTED
                && masterI2C.getSlaveData(data)) {  //т.к. в check_voltage не проверяем crc
                
                print_wifi_mode();
                #if LOGLEVEL>=1
                LOG_START(FPSTR(S_INFO) ,FPSTR(S_WIF)); LOG(F("Connected, IP: ")); LOG(WiFi.localIP().toString());
                #endif
                
                cdata.rssi = WiFi.RSSI();
                #if LOGLEVEL>=2
                LOG_START(FPSTR(S_DEBUG) ,FPSTR(S_WIF)); LOG(F("RSSI: ")); LOG(cdata.rssi);
                #endif

                if (send_blynk(sett, data, cdata)) {
                    #if LOGLEVEL>=1
                    LOG_START(FPSTR(S_INFO) ,FPSTR(S_BLK)); LOG(F("Send OK"));
                    #endif
                }
                
                if (send_mqtt(sett, data, cdata)) {
                    #if LOGLEVEL>=1
                    LOG_START(FPSTR(S_INFO) ,FPSTR(S_MQT)); LOG(F("Send OK"));
                    #endif
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
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_ESP)); LOG(F("Going to sleep"));
    #endif
    
    masterI2C.sendCmd('Z');        // "Можешь идти спать, attiny"
    LOG_END();
    
    ESP.deepSleepInstant(0, RF_DEFAULT);  // Спим до следущего включения EN. Instant не ждет 92мс
}

