#ifndef _SENDERMQTT_h
#define _SENDERMQTT_h

#ifdef SEND_MQTT

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "master_i2c.h"
#include "Logging.h"

bool send_mqtt(const Settings &sett, const SlaveData &data, const float &channel0, const float &channel1)
{
    if (strnlen(sett.mqtt_host, MQTT_HOST_LEN) == 0) {
        LOG_NOTICE( "MQTT", "No host. SKIP");
        return false;
    }

    WiFiClient wclient;   
    PubSubClient client(wclient);
    client.setServer(sett.mqtt_host, sett.mqtt_port);

    String clientId = "waterius-" + WiFi.macAddress();

    if (client.connect(clientId.c_str(), String(sett.mqtt_login).c_str(), String(sett.mqtt_password).c_str())) {
        client.publish(String(sett.mqtt_topic_c0).c_str(), String(channel0).c_str());
        client.publish(String(sett.mqtt_topic_c1).c_str(), String(channel1).c_str());
        client.publish(String(sett.mqtt_topic_bat).c_str(), String((float)(data.voltage / 1000.0)).c_str());
        client.disconnect();
        return true;
    }  else {
        LOG_ERROR("MQTT", "connect error");
    } 

    return false;
}        

#endif //#ifdef SEND_MQTT

#endif
