

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
    LOG_NOTICE(FPSTR(S_ESP), FPSTR(S_BOOTED));
    masterI2C.begin();    //Включаем i2c master
}

/*
Берем начальные показания и кол-во импульсов, 
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    LOG_NOTICE(FPSTR(S_ESP), "new impulses=" << data.impulses0 << " " << data.impulses1);

    if (sett.liters_per_impuls > 0) {
        cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.liters_per_impuls;
        cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.liters_per_impuls;
        LOG_NOTICE(FPSTR(S_ESP), "new value0=" << cdata.channel0 << " value1=" << cdata.channel1);
        
        cdata.delta0  = (data.impulses0 - sett.impulses0_previous)*sett.liters_per_impuls;
        cdata.delta1 = (data.impulses1 - sett.impulses1_previous)*sett.liters_per_impuls;
        LOG_NOTICE(FPSTR(S_ESP), "delta0=" << cdata.delta0 << " delta1=" << cdata.delta1);
    }
}

#define LOW_BATTERY_DIFF_MV 15  //надо еще учесть качество замеров (компаратора у attiny)
#define ALERT_POWER_DIFF_MV 40

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
            LOG_ERROR(FPSTR(S_ESP), FPSTR(S_ERROR_LOAD_CFG));
        }

        //Вычисляем текущие показания
        calculate_values(sett, data, cdata);

        if (mode == SETUP_MODE) { //Режим настройки - запускаем точку доступа на 192.168.4.1
            //Запускаем точку доступа с вебсервером
            WiFi.mode(WIFI_AP_STA);
            setup_ap(sett, data, cdata);

            success = false; // ESP падает после настройки при https, поэтому идём спать. 
                             // в будущем, когда вылечим, ESP будет выходить на связь сразу после настройки
                             // пока не хватает памяти для HTTPS и падение в момент создании объекта
            //WiFi.mode(WIFI_OFF);
            //delay(500);
            //mode = TRANSMIT_MODE;
        }
        
        if (success) {
            if (mode == TRANSMIT_MODE) { 
                //Проснулись для передачи показаний
                LOG_NOTICE(FPSTR(S_WIF), FPSTR(S_STARTING));

                //WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                WiFi.begin(); 
                
                //Ожидаем подключения к точке доступа
                uint32_t start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {
                    LOG_NOTICE(FPSTR(S_WIF), "Status: " << WiFi.status());
                    delay(200);

                    check_voltage(data, cdata);
                }
            }

            //В будущем добавим success, означающее, что напряжение не критично изменяется, можно продолжать
            //иначе есть риск ошибки ESP и стирания конфигурации
            if (WiFi.status() == WL_CONNECTED 
                && masterI2C.getSlaveData(data)) { //тут надо достоверно прочитать i2c

                LOG_NOTICE(FPSTR(S_WIF), "Connected, IP: " << WiFi.localIP().toString());

                if (send_blynk(sett, data, cdata)) {
                    LOG_NOTICE(FPSTR(S_BLK), FPSTR(S_SEND_OK));
                }
                
                if (send_mqtt(sett, data, cdata)) {
                    LOG_NOTICE(FPSTR(S_MQT), FPSTR(S_SEND_OK));
                }

                UserClass::sendNewData(sett, data, cdata);

                //Сохраним текущие значения в памяти.
                sett.impulses0_previous = data.impulses0;
                sett.impulses1_previous = data.impulses1;
                storeConfig(sett);
            }
        }
    }

    LOG_NOTICE(FPSTR(S_ESP), FPSTR(S_GOING_SLEEP));
    masterI2C.sendCmd('Z');        // "Можешь идти спать, attiny"
    LOG_END();
    
    twi_stop();
    ESP.deepSleep(0, RF_DEFAULT);  // Спим до следущего включения EN
}
