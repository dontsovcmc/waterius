
#define UUID_SYSLOG_UDP_IPV4_ARP_MESSAGE_DELAY 1
#define UUID_LOG_MAX_LOG_LENGTH 512UL

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


//#include <uuid/common.h>
//#include <uuid/log.h>
#include <uuid/syslog.h>
#include "LittleFS.h" 


MasterI2C masterI2C; // Для общения с Attiny85 по i2c

SlaveData data; // Данные от Attiny85
Settings sett;  // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; //вычисляемые данные
static uuid::syslog::SyslogService syslog;


/*
Выполняется однократно при включении
*/
void setup()
{
    syslog.hostname(AP_NAME);
	syslog.log_level(uuid::log::ALL);
    syslog.maximum_log_messages(100);
	syslog.mark_interval(0);
	syslog.destination(IPAddress(255, 255, 255, 255));
    //syslog.destination(IPAddress(192, 168, 0, 108));
    syslog.start();
    WiFi.mode(WIFI_OFF);  //TODO  а нужна ли?

    memset(&cdata, 0, sizeof(cdata));
    memset(&data, 0, sizeof(data));
    LOG_BEGIN(115200);    //Включаем логгирование на пине TX, 115200 8N1
    //LOG_INFO(FPSTR(S_ESP), F("Booted"));
    log_handler.start();
    //log_esp.log(uuid::log::INFO, uuid::log::Facility::KERN, F("Booted"));
    log_esp.debug(F("Booted"));
    masterI2C.begin();    //Включаем i2c master
    LittleFS.begin();
}

/*
Берем начальные показания и кол-во импульсов, 
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(const Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    //LOG_INFO(FPSTR(S_ESP), F("new impulses=") << data.impulses0 << " " << data.impulses1);
    log_esp.debug(F("new impulses=%i, %i"), data.impulses0, data.impulses1);

    if ((sett.factor1 > 0) && (sett.factor0 > 0)) {
        cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.factor0;
        cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.factor1;
        //LOG_INFO(FPSTR(S_ESP), F("new value0=") << cdata.channel0 << F(" value1=") << cdata.channel1);
        log_esp.debug(F("new value0=%i value1=%i"), cdata.channel0, cdata.channel1);
        
        cdata.delta0  = (data.impulses0 - sett.impulses0_previous) * sett.factor0;
        cdata.delta1 = (data.impulses1 - sett.impulses1_previous) * sett.factor1;
        //LOG_INFO(FPSTR(S_ESP), F("delta0=") << cdata.delta0 << F(" delta1=") << cdata.delta1);
        log_esp.debug(F("delta0=%i delta1=%i"), cdata.delta0, cdata.delta1);
    }
}

void syslogsendall(){
    uint32_t timeout=millis()+1000;
    while((WiFi.status() == WL_CONNECTED) && (syslog.current_log_messages()) && (millis()<timeout)){
        syslog.loop();
        delay(1);
    }
}

void loop()
{
    uint8_t mode = SETUP_MODE;//TRANSMIT_MODE;
	// спрашиваем у Attiny85 повод пробуждения и данные
    log_i2c.info(F("version\tservice\tvoltage\tresets\tMODEL\tstate0\tstate1\timp0\timp1\tadc0\tadc1\tCRC ok"));
    if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data)) {
        //Загружаем конфигурацию из EEPROM
        bool success = loadConfig(sett);
        if (!success) {
            //LOG_ERROR(FPSTR(S_ESP), F("Error loading config"));
            log_esp.err(F("Error loading config"));
        }
        log_handler.cacheLevel=sett.SaveLogLevel;

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
                //LOG_INFO(FPSTR(S_WIF), F("Starting Wi-fi"));
                log_wif.debug(F("Starting Wi-fi"));
                
                if (sett.ip != 0) {
                    success = WiFi.config(sett.ip, sett.gateway, sett.mask, sett.gateway, IPAddress(8,8,8,8));
                    if (success) {
                        //LOG_INFO(FPSTR(S_WIF), F("Static IP OK"));
                        log_wif.debug(F("Static IP OK"));
                    } else {
                        //LOG_ERROR(FPSTR(S_WIF), F("Static IP FAILED"));
                        log_wif.err(F("Static IP FAILED"));
                    }
                }

                if (success) {
                    if(!masterI2C.setWakeUpPeriod(sett.wakeup_per_min)){
                        //LOG_ERROR(FPSTR(S_I2C), F("Wakeup period wasn't set"));
                        log_i2c.warning(F("Wakeup period wasn't set"));
                    } //"Разбуди меня через..."
                    else{
                        //LOG_INFO(FPSTR(S_I2C), F("Wakeup period, min:") << sett.wakeup_per_min);
                        log_i2c.notice(F("Wakeup period, min: %i"),sett.wakeup_per_min);
                    }

                    //WifiManager уже записал ssid & pass в Wifi, поэтому не надо самому заполнять
                    WiFi.begin(); 

                    //Ожидаем подключения к точке доступа
                    uint32_t start = millis();
                    uint32_t refresh = millis();
                    log_i2c.debug(F("version\tservice\tvoltage\tresets\tMODEL\tstate0\tstate1\timp0\timpu1\tadc0\tadc1\tCRC ok"));
                    while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {
                        //delay(300);
                        yield();
                        if (millis()>refresh){
                            check_voltage(data, cdata);
                            //В будущем добавим success, означающее, что напряжение не критично изменяется, можно продолжать
                            //иначе есть риск ошибки ESP и стирания конфигурации
                            //LOG_INFO(FPSTR(S_WIF), F("Status: ") << WiFi.status());
                            log_wif.debug(F("Status: %i"), WiFi.status());
                            refresh+=300;
                        }
                    }
                }
            }
            syslogsendall();
            if (success 
                && WiFi.status() == WL_CONNECTED
                && masterI2C.getSlaveData(data)) {  //т.к. в check_voltage не проверяем crc
                
				if(!WiFi.hostname((String("Waterius-"+String(ESP.getChipId(), HEX))).c_str())) LOG_INFO(FPSTR(S_WIF), "set hostname fail");
                LOG_INFO(FPSTR(S_WIF), "hostname "+String(WiFi.hostname()));
				
                print_wifi_mode();
                //LOG_INFO(FPSTR(S_WIF), F("Connected, IP: ") << WiFi.localIP().toString());
                log_wif.info(F("Connected, IP: %s"), WiFi.localIP().toString().c_str());
                
                cdata.rssi = WiFi.RSSI();
                //LOG_DEBUG(FPSTR(S_WIF), F("RSSI: ") << cdata.rssi);
                log_wif.debug(F("RSSI: %i"), cdata.rssi);
                syslogsendall();

                if (send_blynk(sett, data, cdata)) {
                    //LOG_INFO(FPSTR(S_BLK), F("Send OK"));
                    log_blk.info(F("Send OK"));
                    syslogsendall();
                }
                
                if (send_mqtt(sett, data, cdata)) {
                    //LOG_INFO(FPSTR(S_MQT), F("Send OK"));
                    log_mqt.info(F("Send OK"));
                    syslogsendall();
                }

                UserClass::sendNewData(sett, data, cdata);
                syslogsendall();

                //Сохраним текущие значения в памяти.
                sett.impulses0_previous = data.impulses0;
                sett.impulses1_previous = data.impulses1;
                //Перешлем время на сервер при след. включении
                sett.wake_time = millis();

                storeConfig(sett);
            }
        } 
    }
    //LOG_INFO(FPSTR(S_ESP), F("Going to sleep"));
    log_esp.debug(F("Going to sleep"));
    syslogsendall();

    File f = LittleFS.open("/log.txt", "w");
    if((mode != SETUP_MODE) && (log_handler.index_r!=log_handler.index_w)){
        if (log_handler.overflow_buffer){
            f.write(&(log_handler.ring_buffer[log_handler.index_w]),log_handler.MAX_BUFFER_SIZE-log_handler.index_w);
        }
        f.write(log_handler.ring_buffer,log_handler.index_w-1);
    }
    f.close();

    LittleFS.end();
    masterI2C.sendCmd('Z');        // "Можешь идти спать, attiny"
    LOG_END();
    ESP.deepSleepInstant(0, RF_DEFAULT);  // Спим до следущего включения EN. Instant не ждет 92мс
}
