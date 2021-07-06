#include "Logging.h"


void log_msg_print(String line){
    /* Generate and print the trailing log timestamp.
		1 = (1234), showing time in seconds since boot. Generates lightweight inline code.
		2 = (001:02:03:04:005) ( day 1, hour 2, minute 3, second 4, millisecond 5 ). Much bigger code, but very readable
    */
    #if LOG_TIME_FORMAT == 1
	Serial.print(String( millis() / 1000));
    #elif LOG_TIME_FORMAT == 2
	unsigned long logTime = millis();
    unsigned short ms = logTime % 1000 ;
    logTime = logTime / 1000;
    unsigned char seconds = logTime % 60 ;
    logTime = logTime / 60;
	unsigned char minutes = logTime % 60 ;
    logTime = logTime / 60;
	unsigned char hours = logTime % 24 ;
    logTime = logTime / 24;
    unsigned short days = logTime ;

	char logFormattedTime[17]; 
    sprintf( logFormattedTime, "%03u:%02u:%02u:%02u:%03u", days, hours, minutes, seconds, ms ); 
	Serial.print(logFormattedTime); 
    #endif
    Serial.println(line);
};

void LOG_BEGIN(unsigned long baud){
    #ifdef LOGLEVEL
    Serial.begin(baud, SERIAL_8N1);
    #endif
};

void LOG_END(){
    #ifdef LOGLEVEL
    Serial.flush(); 
    Serial.end();
    #endif
};

void LOG_ERROR(String svc, String content){
    #ifdef LOGLEVEL
    log_msg_print("  ERROR     (" + String(svc) + ") : " + String(content)); 
    #endif
};

void LOG_INFO(String svc, String content){ 
    #if LOGLEVEL >= 1
    log_msg_print("  INFO      (" + String(svc) + ") : " + String(content)); 
    #endif
};

void LOG_DEBUG(String svc, String content){ 
    #if LOGLEVEL >= 2
    log_msg_print("  DEBUG     (" + String(svc) + ") : " + String(content)); 
    #endif
};

