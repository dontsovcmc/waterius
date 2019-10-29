#include <Arduino.h>

#define LOGLEVEL 6

#include "Logging.h"
#include "wbutton.h"
#include "setup_ap.h"
#include "transmit.h"

#define LED_PIN    GPIO_NUM_4
#define CAMERA_PIN GPIO_NUM_32
#define BUTTON_PIN GPIO_NUM_7

void setup()
{
    LOG_BEGIN(115200);
    delay(1000); //Take some time to open up the Serial Monitor
    
    pinMode(OUTPUT_OPEN_DRAIN, GPIO_NUM_4); // LED_FLASH
    pinMode(OUTPUT_OPEN_DRAIN, GPIO_NUM_12); // safe 50uA
    pinMode(INPUT_PULLUP, GPIO_NUM_14);  //ресет камеры
    pinMode(OUTPUT_OPEN_DRAIN, GPIO_NUM_15); // safe 50uA
    pinMode(OUTPUT_OPEN_DRAIN, GPIO_NUM_32); 

    LOG_INFO("SYS", "Going to sleep now");
    Serial.flush(); 
    ESP.deepSleep(0);
}

void loop()
{ }