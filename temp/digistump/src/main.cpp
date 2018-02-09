

//#define USE_BUTTON2

#include "sleep_counter.h"

#include <TinyDebugSerial.h>

TinyDebugSerial mySerial;
static uint32_t counter = 0;
static uint32_t counter2 = 0;

#define DEBUG

#ifdef DEBUG
  #define DEBUG_CONNECT(x)  mySerial.begin(x)
  #define DEBUG_PRINT(x)    mySerial.print(x)
  #define DEBUG_PRINTLN(x)    mySerial.println(x)
  #define DEBUG_FLUSH()     mySerial.flush()
#else
  #define DEBUG_CONNECT(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x) 
  #define DEBUG_FLUSH()
#endif


void setup() 
{
    // put your setup code here, to run once:
    resetWatchdog(); // Needed for deep sleep to succeed

    DEBUG_CONNECT(9600);
  	DEBUG_PRINTLN(F("==== START ===="));

}

void loop() 
{
    // put your main code here, to run repeatedly:
    gotoDeepSleep(2, &counter, &counter2);		// Deep sleep for X seconds

    DEBUG_PRINT("1: counter=");
    DEBUG_PRINT(counter);
#ifdef BUTTON2_PIN
    DEBUG_PRINT(", counter2=");
    DEBUG_PRINTLN(counter2);
#endif
    delay(2000);

    DEBUG_PRINT("2: counter=");
    DEBUG_PRINT(counter);
#ifdef BUTTON2_PIN
    DEBUG_PRINT(", counter2=");
    DEBUG_PRINTLN(counter2);
#endif
}