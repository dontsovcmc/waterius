#ifndef _SENDERTCP_h
#define _SENDERTCP_h

#include "Setup.h"

#include "MasterI2C.h"
#include "Logging.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

WiFiClient client;

struct SenderTCP {
public:

    static bool send(const Settings &sett, const SlaveData &data)    {

        LOG_NOTICE( "WIF", "Starting Wifi" );
        IPAddress ip(sett.ip);
        IPAddress gw(sett.gw);
        IPAddress subnet(sett.subnet);
        WiFi.config( ip, gw, subnet );
        WiFi.begin();

        uint32_t now = millis();
        while ( WiFi.status() != WL_CONNECTED && millis() - now < ESP_CONNECT_TIMEOUT)  {

            LOG_NOTICE( "WIF", "Wifi status: " << WiFi.status());
            delay(100);
        }

        if (millis() - now < ESP_CONNECT_TIMEOUT)
        {
            LOG_NOTICE( "WIF", "Wifi connected, got IP address: " << WiFi.localIP().toString() );
            
            IPAddress ip;
            ip.fromString( sett.hostname );
            LOG_NOTICE( "WIF", "Making TCP connection to " << ip.toString() );

            client.setTimeout( SERVER_TIMEOUT ); 
            if (client.connect( ip, 4001 ) ) {

                int length = sizeof(SlaveData);
                LOG_NOTICE( "WIF", "Sending " << length << " bytes of data" );
                uint16_t bytesSent = client.write( (char*)&data, length );

                client.stop();
                if ( bytesSent == length ) {
                    LOG_NOTICE( "WIF", "Data sent successfully" );
                    return true;
                }
                LOG_ERROR( "WIF", "Could not send data" );
                
            } else {
                LOG_ERROR( "WIF", "Connection to server failed" );
            }
        } else {

            LOG_ERROR( "WIF", "Wifi connect error");
        }
        return false;
    }
};

#endif