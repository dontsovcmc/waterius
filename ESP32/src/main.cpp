#include <Arduino.h>
#include "wbutton.h"
#include "setup_ap.h"
#include "transmit.h"

#define LED_PIN    GPIO_NUM_32
#define BUTTON_PIN GPIO_NUM_7


#define TIME_TO_SLEEP_SEC  2 //60        // Time ESP32 will go to sleep (in seconds)

#define TRANSMIT_PERIOD_MIN  3 //60*24   // 1 day

RTC_DATA_ATTR int bootCount = 0;

WateriusButton button(BUTTON_PIN);

void init_wakeup()
{
    Serial.printf("Init timer wakeup"); 
    bootCount = 0;
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_SEC * 1000000);
    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, HIGH);

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_SEC * 1000000);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP_SEC) + " Seconds");

}

void disable_wakeup()
{
    Serial.printf("Disable wakeup");
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
}

void setup()
{
    Serial.begin(115200);
    delay(1000); //Take some time to open up the Serial Monitor

    //Increment boot number and print it every reboot
    ++bootCount;
    Serial.println("Boot number: " + String(bootCount));

    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason)
    {
        case ESP_SLEEP_WAKEUP_EXT1: 
            disable_wakeup();

            Serial.println("Wakeup caused by external signal using RTC_CNTL"); 

            if (button.long_press()) 
            {
                setup_ap();
            }
            else
            {
                transmit_data();
            }
            
            init_wakeup();
            break;
        case ESP_SLEEP_WAKEUP_TIMER: 
            Serial.println("Wakeup caused by timer"); 

            if (bootCount > TRANSMIT_PERIOD_MIN) {
                transmit_data();
                init_wakeup();
            }
            break;
        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
        default: 
            Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); 
            init_wakeup();
            break;
    }
    Serial.println("Going to sleep now");
    Serial.flush(); 
    esp_deep_sleep_start();
    Serial.println("This will never be printed");
}



void loop()
{
    
}