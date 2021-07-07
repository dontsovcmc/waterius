#include "Logging.h"


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

void LOG_START(String group, String svc){
    /* Generate and print the trailing log timestamp.
		1 = (1234), showing time in seconds since boot. Generates lightweight inline code.
		2 = (001:02:03:04:005) ( day 1, hour 2, minute 3, second 4, millisecond 5 ). Much bigger code, but very readable
    */
    Serial.println(""); 
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
	Serial.printf("%03u:%02u:%02u:%02u:%03u", days, hours, minutes, seconds, ms ); 
    #endif
    LOG(group);
    LOG(svc);
    LOG(S_SVC);
}

