#ifndef _SENDERTCP_h
#define _SENDERTCP_h

#include "Setup.h"

#include "master_i2c.h"
#include "Logging.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#ifdef SEND_TCP

#define TCP_SERVER_PORT 4001

WiFiClient client;

struct SendData {
    float value0;
    float value1;
    float voltage;
};

bool send_tcp(const Settings &sett, const float &value0, const float &value1, const float &voltage)
{
    bool ret = false;
    if (WiFi.status() != WL_CONNECTED) {
        LOG_ERROR("TCP", "Wi-Fi is not connected");
        return ret;
    }
    
    client.setTimeout(SERVER_TIMEOUT); 

    IPAddress ip;
    bool connect = false;

    if (ip.fromString(sett.hostname)) {

        LOG_NOTICE("WIF", "Making TCP connection to ip " << ip.toString() << " port " << TCP_SERVER_PORT);
        connect = client.connect(ip, 4001); 
    } else {

        LOG_NOTICE("WIF", "Making TCP connection to host " << sett.hostname << " port " << TCP_SERVER_PORT);
        connect = client.connect(sett.hostname, TCP_SERVER_PORT);
    }

    if (connect) {
        SendData data;
        data.value0 = value0;
        data.value1 = value1;
        data.voltage = voltage;

        uint16_t bytesSent = client.write((char*)&data, sizeof(data));

        client.stop();
        if (bytesSent == sizeof(data)) {
            LOG_NOTICE("WIF", "Data sent successfully");
            return true;
        }
        LOG_ERROR("WIF", "Could not send data");
        
    } else {
        LOG_ERROR("WIF", "Connection to server failed");
    }
        
    return false;
}

#endif //#ifdef SEND_TCP

#endif
