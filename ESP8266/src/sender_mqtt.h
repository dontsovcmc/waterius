#ifndef _SENDERMQTT_h
#define _SENDERMQTT_h

#ifdef SEND_MQTT

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "master_i2c.h"
#include "Logging.h"

String mqtt_get_full_topic(const Settings &sett, String topicName)
{
    String topic = String(sett.mqtt_topic);
    if (topic.charAt(topic.length()) != '/') topic += '/';
    topic += topicName;
    return topic;
}

bool send_mqtt(const Settings &sett, const SlaveData &data, const float &channel0, const float &channel1)
{
    if (strnlen(sett.mqtt_host, MQTT_HOST_LEN) == 0) {
        LOG_NOTICE( "MQTT", "No host. SKIP");
        return false;
    }

    WiFiClient wclient;   
    PubSubClient client(wclient);
    client.setServer(sett.mqtt_host, sett.mqtt_port);

    String clientId = "waterius-";
    clientId += ESP.getChipId();

    unsigned int delta0 = (data.impulses0 - sett.impulses0_previous)*sett.liters_per_impuls; // litres
    unsigned int delta1 = (data.impulses1 - sett.impulses1_previous)*sett.liters_per_impuls; 

    if (client.connect(clientId.c_str(), String(sett.mqtt_login).c_str(), String(sett.mqtt_password).c_str())) {
        client.publish(mqtt_get_full_topic(sett, "ch0").c_str(), String(channel0).c_str());
        client.publish(mqtt_get_full_topic(sett, "ch1").c_str(), String(channel1).c_str());
        client.publish(mqtt_get_full_topic(sett, "delta0").c_str(), String(delta0).c_str());
        client.publish(mqtt_get_full_topic(sett, "delta1").c_str(), String(delta1).c_str());
        client.publish(mqtt_get_full_topic(sett, "voltage").c_str(), String((float)(data.voltage / 1000.0)).c_str());
        client.publish(mqtt_get_full_topic(sett, "resets").c_str(), String(data.resets).c_str());
        client.publish(mqtt_get_full_topic(sett, "good").c_str(), String(data.diagnostic).c_str());
        client.publish(mqtt_get_full_topic(sett, "boot").c_str(), String(data.version).c_str());
        client.publish(mqtt_get_full_topic(sett, "imp0").c_str(), String(data.impulses0).c_str());
        client.publish(mqtt_get_full_topic(sett, "imp1").c_str(), String(data.impulses1).c_str());
        client.publish(mqtt_get_full_topic(sett, "version").c_str(), String(sett.version).c_str());
        client.publish(mqtt_get_full_topic(sett, "version_esp").c_str(), String(FIRMWARE_VERSION).c_str());
        client.publish(mqtt_get_full_topic(sett, "resets").c_str(), String(data.resets).c_str());

        client.disconnect();
        return true;
    }  else {
        LOG_ERROR("MQTT", "connect error");
    } 

    return false;
}        

#endif //#ifdef SEND_MQTT

#endif
