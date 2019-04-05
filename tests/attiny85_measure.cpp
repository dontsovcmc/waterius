#include <Arduino.h>
#include "TinyDebugSerial.h"

TinyDebugSerial mySerial;

#define PRINT_NOW(x) mySerial.print(millis()); mySerial.print(x);
#define LOG(x) { PRINT_NOW(F(": ")); mySerial.println(x); }

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define INPUT1 3
#define INPUT2 4

unsigned long t = 0;
unsigned long count = 0;

int i = 0;
uint16_t result = 0;
uint16_t result2 = 0;

void setup() {
  // put your setup code here, to run once:
  mySerial.begin(9600);

  LOG("START");

  DDRB &= ~_BV(3); 
  DDRB &= ~_BV(4);  
  
}



void loop() {
  // put your main code here, to run repeatedly:

  t = millis();
  count = 0;
  while (millis() - t < 1000) {

    // ДИСКРЕТНЫЙ ВХОД

    

/*
    //4962 1 вход
    //1985 2 входа
    pinMode(INPUT1, INPUT_PULLUP);
    i = digitalRead(INPUT2);
    pinMode(INPUT2, INPUT);
    //pinMode(INPUT2, INPUT_PULLUP);
    //i = digitalRead(INPUT1);
    //pinMode(INPUT1, INPUT);
*/
/*
 
    //17321 1 вход
    //15881 2 входа    
    DDRB &= ~_BV(3); 
    DDRB &= ~_BV(4);  
    
    PORTB |= _BV(3);  // INPUT_PULLUP
    i = bit_is_clear(PINB, 3);
    PORTB &= ~_BV(3); //INPUT
 
    PORTB |= _BV(4);  // INPUT_PULLUP
    i = bit_is_clear(PINB, 4);
    PORTB &= ~_BV(4); //INPUT
*/
    // АНАЛОГОВЫЙ ВХОД
 
/*
    // 1 вход 2564
    // 2 входа 1326

    pinMode(INPUT1, INPUT_PULLUP);
    i = analogRead(INPUT1);
    pinMode(INPUT1, INPUT);
*/   

    // 1 вход 5037
    // 2 входа 3052
    
    PORTB |= _BV(3);  // INPUT_PULLUP


    ADMUX = (ADMUX & 0xF0) | (3 & 0x0F); //PB3

    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA,ADSC)); // measuring

    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH 
    uint8_t high = ADCH; // unlocks both

    result = (high<<8) | low;
    PORTB &= ~_BV(3); 
    DDRB &= ~_BV(3); 

/*
    /PORTB |= _BV(4);  // INPUT_PULLUP
    ADMUX = (ADMUX & 0xF0) | (2 & 0x0F);  //PB4
    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA,ADSC)); // measuring

    low  = ADCL; // must read ADCL first - it then locks ADCH 
    high = ADCH; // unlocks both

    result2 = (high<<8) | low;
    PORTB &= ~_BV(4); 
    DDRB &= ~_BV(4);  
*/

    count++;
  }

  mySerial.begin(9600);
  LOG(count);
  LOG("VOLTAGE");
  LOG(result);
  LOG(result2);
}