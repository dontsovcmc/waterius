#ifndef _SENDERTCP_h
#define _SENDERTCP_h

#include "setup.h"

#include "master_i2c.h"
#include "Logging.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#ifdef SEND_TCP

#define TCP_SERVER_PORT 4001

WiFiClient client;

/*
Структура данных, которую хотим отправить на TCP сервер.
*/
struct SendData {
    float channel0;
    float channel1;
    float voltage;
};

/*
Функция отправляющая данные на TCP сервер.
Ip адрес или имя сайта: sett.hostname
Порт: TCP_SERVER_PORT 4001
*/
bool send_tcp(const Settings &sett, const SlaveData &data, const float &channel0, const float &channel1)
{
    client.setTimeout(SERVER_TIMEOUT); 

    IPAddress ip;
    bool connect = false;

    if (ip.fromString(sett.hostname)) {

        LOG_NOTICE("TCP", "Making TCP connection to ip " << ip.toString() << " port " << TCP_SERVER_PORT);
        connect = client.connect(ip, TCP_SERVER_PORT); 
    } else {

        LOG_NOTICE("TCP", "Making TCP connection to host " << sett.hostname << " port " << TCP_SERVER_PORT);
        connect = client.connect(sett.hostname, TCP_SERVER_PORT);
    }

    if (connect) {
        SendData d;
        d.channel0 = channel0;
        d.channel1 = channel1;
        d.voltage = data.voltage / 1000.0;

        uint16_t bytesSent = client.write((char*)&d, sizeof(d));

        client.stop();
        if (bytesSent == sizeof(d)) {
            LOG_NOTICE("TCP", "Data sent successfully");
            return true;
        }
        LOG_ERROR("TCP", "Could not send data");
        
    } else {
        LOG_ERROR("TCP", "Connection to server failed");
    }
        
    return false;
}

#endif //#ifdef SEND_TCP

#endif
