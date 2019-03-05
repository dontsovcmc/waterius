

#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>

#include "wifi_settings.h"
#include "master_i2c.h"
#include "setup_ap.h"
#include "sender_blynk.h"
#include "sender_https.h"
#include "ota_update.h"
#include "WiFiClient.h"
#include "utils.h"


MasterI2C masterI2C; // Для общения с Attiny85 по i2c

SlaveData data; // Данные от Attiny85
Settings sett;  // Настройки соединения и предыдущие показания из EEPROM

StaticJsonBuffer<1000> jsonBuffer;

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

BearSSL::WiFiClientSecure client_tls;
WiFiClient client;
BearSSL::X509List cert;
HTTPClient http;

void loop()
{
    float channel0, channel1;
    uint8_t mode = TRANSMIT_MODE;

	// спрашиваем у Attiny85 повод пробуждения и данные
    //if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data)) {
    if (true) {
        if (mode == SETUP_MODE) {
            //Режим настройки - запускаем точку доступа на 192.168.4.1

            //Загружаем конфигурацию из EEPROM
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
                    if (send_blynk(sett, data, channel0, channel1)) {
                        LOG_NOTICE("BLK", "send ok");
                    }
#endif  
                    bool https = strstr(sett.hostname_json, "https") > 0;

                    if (!https || setClock()) {
                        WiFiClient *c = &client;
                        if (https) {
                            cert.append(lets_encrypt_x3_ca);
                            cert.append(lets_encrypt_x4_ca);
                            cert.append(cloud_waterius_ru_ca);
                    
                            client_tls.setTrustAnchors(&cert);
                            c = &client_tls;
                        } 
                        
                        c->setTimeout(SERVER_TIMEOUT);

                        /*
                        bool mfln = client.probeMaxFragmentLength("192.168.1.42", 5000, 1024);  // server must be the same as in ESPhttpUpdate.update()
                        Serial.printf("MFLN supported: %s\n", mfln ? "yes" : "no");
                        if (mfln) {
                            client.setBufferSizes(1024, 1024);
                        }*/

                        
                        //http.setTimeout(SERVER_TIMEOUT);

                        ESP.resetFreeContStack();
                        LOG_NOTICE("SYS", "Stack: " << ESP.getFreeContStack());

                        http.setReuse(false); //only 1 request
                        
                        JsonObject& root = jsonBuffer.createObject();
                        String responce;
                        String request;

                        prepareJson(root, sett, data, channel0, channel1);
                        root.printTo(request);

                        LOG_NOTICE("JSN", "json: " << request);
                        LOG_NOTICE("JSN", "POST to: " << sett.hostname_json);
                        if (http.begin(*c, sett.hostname_json)) {  // hostname); 
                            http.addHeader("Content-Type", "application/json"); 

                            int httpCode = http.POST(request);   //Send the request
                            
                            LOG_NOTICE("JSN", httpCode);
                            if (httpCode == HTTP_CODE_OK) {
                                responce = http.getString();
                                LOG_NOTICE("JSN", "Responce: \n" << responce);
                                JsonObject& root = jsonBuffer.parseObject(responce);
                                if (!root.success()) {
                                    LOG_NOTICE("JSN", responce);
                                } else {
                                    LOG_ERROR("JSN", "parse response error");
                                }
                            } 
                        }

                        c->stop();
                    }

                }

                //Сохраним текущие значения в памяти.
                sett.channel0_previous = channel0;
                sett.channel1_previous = channel1;
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
