

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

StaticJsonBuffer<2000> jsonBuffer;
String request;

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

BearSSL::WiFiClientSecure client;
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

                    if (setClock()) {

                        cert.append(digicert);
                        if (strlen(sett.ca) > 500) {
                            LOG_ERROR("TLS", "add CA2 certificate");
                            cert.append(sett.ca);
                        } else {
                            LOG_ERROR("TLS", "not CA2");
                        }

                        client.setTrustAnchors(&cert);
                        //client.setInsecure();
                        
                        /*
                        bool mfln = client.probeMaxFragmentLength("192.168.1.42", 5000, 1024);  // server must be the same as in ESPhttpUpdate.update()
                        Serial.printf("MFLN supported: %s\n", mfln ? "yes" : "no");
                        if (mfln) {
                            client.setBufferSizes(1024, 1024);
                        }*/

                        client.setTimeout(20000);
                        
                        http.setTimeout(SERVER_TIMEOUT);

                        ESP.resetFreeContStack();
                        LOG_NOTICE("SYS", "Stack: " << ESP.getFreeContStack());

                        http.setReuse(true);
                        
                        /*
                        if (http.begin(client, host, port, "/ping")) {  // hostname); 
                            LOG_NOTICE("GET", "waterius connected");
                            http.addHeader("Content-Type", "application/json"); 

                            int httpCode = http.GET(); 
                            LOG_NOTICE("GET", httpCode);
                            if (httpCode == HTTP_CODE_OK) {
                                //String responce = http.getString();
                            } 
                        } */
                        
                        JsonObject& root = jsonBuffer.createObject();
                        String responce;
                        String request;

                        prepareJson(root, sett, data, channel0, channel1);
                        root.printTo(request);

                        LOG_NOTICE("JSN", "json: " << request);

                        /*
                        client.connect(host, port);
                        if (!client.connected()) {
                            Serial.printf("*** Can't connect. ***\n-------\n");
                            return;
                        }
                        char len[7];
                        itoa(request.length(), len, 10);
                        Serial.printf("Connected!\n-------\n");
                        client.write("POST ");
                        client.write("/");
                        client.write(" HTTP/1.0");
                        client.write("\r\nHost: ");
                        client.write(host);
                        client.write("\r\nConnection: keep-alive");
                        client.write("\r\nContent-Length: ");
                        client.write(len);
                        client.write("\r\nContent-Type: application/json");
                        client.write("\r\n\r\n");
                        client.write(request.c_str());

                        uint32_t to = millis() + SERVER_TIMEOUT;
                        if (client.connected()) {
                            do {
                                char tmp[40];
                                memset(tmp, 0, 40);
                                int rlen = client.read((uint8_t*)tmp, sizeof(tmp) - 1);
                                yield();
                                if (rlen < 0) {
                                    break;
                                }
                                Serial.print(tmp);
                            } while (millis() < to);
                        }
                        */
                        
                        LOG_NOTICE("JSN", "Try POST: " << sett.hostname_json);
                        if (http.begin(client, sett.hostname_json)) {  // hostname); 
                            http.addHeader("Content-Type", "application/json"); 

                            int httpCode = http.POST(request);   //Send the request
                            
                            LOG_NOTICE("JSN", httpCode);
                            if (httpCode == HTTP_CODE_OK) {
                                responce = http.getString();
                                LOG_NOTICE("JSN", "Responce: \n" << responce);
                                JsonObject& root = jsonBuffer.parseObject(responce);
                                if (!root.success()) {
                                    if (root.containsKey("ca2")) {
                                        strncpy0(sett.ca, root["ca2"], CERT_LEN);
                                        LOG_NOTICE("JSN", "Key CA2 updated");
                                    } else {
                                        LOG_NOTICE("JSN", "No CA2 in response");
                                        LOG_NOTICE("JSN", responce);
                                    }
                                } else {
                                    LOG_ERROR("JSN", "parse response error");
                                }

                                if (root.containsKey("firmware_path")) {
                                    
                                    auto path = root["firmware_path"].as<char*>();
                                    LOG_ERROR("OTA", "firmware_path found " << path);
                                    //Сохраним текущие значения в памяти.
                                    sett.channel0_previous = channel0;
                                    sett.channel1_previous = channel1;
                                    storeConfig(sett);
                                    LOG_ERROR("SYS", "config saved");

                                    LOG_ERROR("OTA", "start");
                                    //ota_update(client, host, port, path);
                                }

                            } 
                        }

                        client.stop();
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
