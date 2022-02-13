#include "Logging.h"
#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266WiFi.h>
#else
# include <WiFi.h>
#endif
#include <WiFiUdp.h>
#include <time.h>


void SerialLogHandler::start() {
		uuid::log::Logger::register_handler(this, uuid::log::Level::ALL);
        ring_buffer = new char[MAX_BUFFER_SIZE];
	}

/*
    * It is not recommended to directly output from this function,
    * this is only a simple example. Messages should normally be
    * queued for later output when the application is less busy.
    */
void SerialLogHandler::operator<<(std::shared_ptr<uuid::log::Message> message) {
    char temp[512] = { 0 };
    unsigned long timestamp_ms = message->uptime_ms;
	unsigned int minutes, seconds, ms;

	timestamp_ms %= 3600000UL;

	minutes = timestamp_ms / 60000UL;
	timestamp_ms %= 60000UL;

	seconds = timestamp_ms / 1000UL;
	timestamp_ms %= 1000UL;

	ms = timestamp_ms;

	char logFormattedTime[17]; \
	sprintf( logFormattedTime, "%04u:%02u.%03u", minutes, seconds, ms ); \


    int ret = snprintf_P(temp, sizeof(temp), PSTR("%s %c [%S] %s\r\n"),
        logFormattedTime,
        uuid::log::format_level_char(message->level),
        message->name, 
        message->text.c_str());

    if (ret > 0) {
        Serial.print(temp);
        
        if (message->level>(uuid::log::Level)cacheLevel) return;

        size_t l=strlen(temp);
        char* w=&ring_buffer[index_w];
        index_w+=l;
        if (index_w>=MAX_BUFFER_SIZE){
            index_w-=MAX_BUFFER_SIZE;
            overflow_buffer=true;
        }
        char* s=&temp[0];
        char *e=&ring_buffer[MAX_BUFFER_SIZE-1];
        while(l>0){
            *w =*s;
            w++;
            s++;
            if (w>e){
                w=&ring_buffer[0];
            }
            l--;
        }
        if (overflow_buffer && index_r<index_w){
            index_r=index_w+1;
            if (index_r>=MAX_BUFFER_SIZE){
                index_r=index_r-MAX_BUFFER_SIZE;
                overflow_buffer=false;
            }
        }
    }
}    