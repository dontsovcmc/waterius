#include <Arduino.h>

#include "master_i2c.h"
#include "logging.h"


#define PIN_COUNTER0 D5
#define PIN_COUNTER1 D6
#define PIN_DETECT_POWER D3
#define PIN_VIRT_BUTTON SCL


MasterI2C masterI2C; // Для общения с Attiny85 по i2c

uint16_t wakeup_per_min = 5;

uint32_t impulses0 = 0;
uint32_t impulses1 = 0;

SlaveData prev_data;


bool communicate(SlaveData &data)
{
  LOG_INFO("Communicate");

  memset(&data, 0, sizeof(data));

  masterI2C.begin();    //Включаем i2c master

  uint8_t mode = SETUP_MODE;  // TRANSMIT_MODE;

  if (masterI2C.getMode(mode) && masterI2C.getSlaveData(data)) { 
    if (mode == SETUP_MODE) {   // Режим настройки 
        
        uint32_t start = millis();
        #define SETUP_TIME 2000
        while (millis() - start < SETUP_TIME) {
            delay(300);
        }
    }
    if (mode == TRANSMIT_MODE) { // Проснулись для передачи показаний

        if(!masterI2C.setWakeUpPeriod(wakeup_per_min)){
            LOG_ERROR(F("Wakeup period wasn't set"));
        } //"Разбуди меня через..."
        else{
            LOG_INFO(F("Wakeup period, min:") << wakeup_per_min);
        }

        uint32_t start = millis();
        #define ESP_CONNECT_TIMEOUT 2000
        while (millis() - start < ESP_CONNECT_TIMEOUT) {
            delay(300);
        }
        
        if (masterI2C.getSlaveData(data)) {  //т.к. в check_voltage не проверяем crc
            
        }
    } 
  }
  else {
    return false;
  }
  LOG_INFO(F("Going to sleep"));
  masterI2C.sendCmd('Z');        // "Можешь идти спать, attiny"

  delay(100);

  return true;
}

void setup() {
  LOG_BEGIN(115200);
  LOG_INFO("Wait First wake up");

  pinMode(PIN_DETECT_POWER, INPUT);
  pinMode(PIN_COUNTER0, OUTPUT);
  pinMode(PIN_COUNTER1, OUTPUT);

  while (digitalRead(PIN_DETECT_POWER) != HIGH)
    ;
  LOG_INFO("First wake up OK");

  communicate(prev_data);
  impulses0 = prev_data.impulses0;
  impulses1 = prev_data.impulses1;

  LOG_INFO("Start impulse0: " << impulses0);
  LOG_INFO("Start impulse1: " << impulses1);

  delay(500);
}

void impulse(uint8_t pin, uint32_t &counter) {
  digitalWrite(pin, HIGH);
  delay(random(500, 15000));
  digitalWrite(pin, LOW);
  ++counter;
}

void check(SlaveData &data) {

  if (data.impulses0 != impulses0) {
    LOG_ERROR("we: " << impulses0 << " attiny: " << data.impulses0);
  }
  if (data.impulses1 != impulses1) {
    LOG_ERROR("we: " << impulses1 << " attiny: " << data.impulses1);
  }

  if (data.resets != prev_data.resets) {
    LOG_ERROR("resets: " << data.resets);
  } 

}

void loop() {
  
  if (digitalRead(PIN_DETECT_POWER) == HIGH) {
    
    wakeup_per_min = random(1, 100);

    SlaveData data;
    if (communicate(data)) {
      check(data);
      prev_data = data;
    } else {
      LOG_ERROR("Communicate error");
    }

  }

  impulse(PIN_COUNTER0, impulses0);
  impulse(PIN_COUNTER1, impulses1);
  delay(random(500, 5000));
}