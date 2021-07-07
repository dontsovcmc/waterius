#ifndef _SENDERMQTT_h
#define _SENDERMQTT_h


#ifdef MQTT_SOCKET_TIMEOUT
#undef MQTT_SOCKET_TIMEOUT
#endif

#define MQTT_SOCKET_TIMEOUT 5


#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "master_i2c.h"
#include "Logging.h"


bool send_mqtt(const Settings &sett, const SlaveData &data, const CalculatedData &cdata)
{
    if (strnlen(sett.mqtt_host, MQTT_HOST_LEN) == 0) {
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_MQT)); LOG(F("SKIP"));
        #endif
        return false;
    }

    WiFiClient wclient;   
    PubSubClient client(wclient);
    client.setServer(sett.mqtt_host, sett.mqtt_port);

    String clientId = "waterius-" + String(ESP.getChipId());

    const char *login = strnlen(sett.mqtt_login, MQTT_LOGIN_LEN) ? sett.mqtt_login : NULL;
    const char *pass = strnlen(sett.mqtt_password, MQTT_PASSWORD_LEN) ? sett.mqtt_password : NULL;
    
    String topic(sett.mqtt_topic);
    if (!topic.endsWith("/"))
        topic += '/';
    
    if (client.connect(clientId.c_str(), login, pass)) {
        client.publish((topic + "ch0").c_str(), String((float)cdata.channel0,3).c_str(), true);
        client.publish((topic + "ch1").c_str(), String((float)cdata.channel1,3).c_str(), true);
        client.publish((topic + "delta0").c_str(), String(cdata.delta0).c_str(), true);
        client.publish((topic + "delta1").c_str(), String(cdata.delta1).c_str(), true);
        client.publish((topic + "voltage").c_str(), String((float)(data.voltage / 1000.0), 3).c_str(), true);
        client.publish((topic + "resets").c_str(), String(data.resets).c_str(), true);
        client.publish((topic + "model").c_str(), String(data.model).c_str(), true);
        client.publish((topic + "boot").c_str(), String(data.service).c_str(), true);
        client.publish((topic + "good").c_str(), String(data.diagnostic).c_str(), true);
        client.publish((topic + "imp0").c_str(), String(data.impulses0).c_str(), true);
        client.publish((topic + "imp1").c_str(), String(data.impulses1).c_str(), true);
        client.publish((topic + "version").c_str(), String(data.version).c_str(), true);
        client.publish((topic + "version_esp").c_str(), String(FIRMWARE_VERSION).c_str(), true);
        client.publish((topic + "voltage_low").c_str(), String(cdata.low_voltage).c_str(), true);
        client.publish((topic + "voltage_diff").c_str(), String((float)(cdata.voltage_diff / 1000.0), 3).c_str(), true);
        client.publish((topic + "f1").c_str(), String(sett.factor1).c_str(), true);
        client.publish((topic + "f0").c_str(), String(sett.factor0).c_str(), true);
        client.publish((topic + "rssi").c_str(), String(cdata.rssi).c_str(), true);
        client.publish((topic + "waketime").c_str(), String(sett.wake_time).c_str(), true);
        client.publish((topic + "setuptime").c_str(), String(sett.setup_time).c_str(), true);
        client.publish((topic + "adc0").c_str(), String(data.adc0).c_str(), true);
        client.publish((topic + "adc1").c_str(), String(data.adc1).c_str(), true);
        client.publish((topic + "period_min").c_str(), String(sett.wakeup_per_min).c_str(), true);
        client.publish((topic + "serial0").c_str(), String(sett.serial0).c_str(), true);
        client.publish((topic + "serial1").c_str(), String(sett.serial1).c_str(), true);
        
        client.disconnect();
        return true;
    }  else {
        #if LOGLEVEL>=0
        LOG_START(FPSTR(S_ERROR) ,FPSTR(S_MQT)); LOG(F("Connect error"));
        #endif
    } 

    return false;
}        

#endif
