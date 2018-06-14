#include "MasterI2C.h"
#include "Logging.h"

#define LOGLEVEL 6

/*
Проект для тестирования счетчика импульсов на attiny85. 
Подает импульсы на вход, при подаче HIGH на DETECT_PIN включает i2c Master
и принимает данные от attiny85.
*/

#define I2C_SLAVE_ADDR 10

#define DETECT_PIN 9  //включение мастера. вместо пина EN (pin 1 attiny85)

#define OUTPUT_1 A0   //к входу счетчика value0 
#define OUTPUT_2 A1   //к входу счетчика value1

#define RESET_PIN A3  //на ресет attiny85

MasterI2C masterI2C;


void setup() {

	pinMode(DETECT_PIN, INPUT);
	pinMode(OUTPUT_1, OUTPUT);
	pinMode(OUTPUT_2, OUTPUT);
    pinMode(RESET_PIN, OUTPUT);
    digitalWrite(RESET_PIN, HIGH);
    
	Serial.begin(115200);
    
	LOG_NOTICE( "ESP", "Booted" );
}

SlaveData data;

void loop() {

    static unsigned long i=0;
    static unsigned long sec=0;
    static unsigned long step=0;
    static unsigned long reset=millis();

    //attiny подало питание на ESP
    if (digitalRead(DETECT_PIN) == HIGH) {

        Serial.print('.');
        Serial.print(sec);
        unsigned long start_power = millis();
		Serial << "\n";
        LOG_NOTICE( "ESP", "powe on I2C-begined" );

        masterI2C.begin();
        
        size_t retries = 5;
        while (retries--) {
            if (!masterI2C.getSlaveData(data)) {
                LOG_ERROR("from power on:", millis() - start_power);
                LOG_ERROR("Error reading slave data, retry:", 40 - retries);
                break;
            }
            uint8_t pause = random(2, 20);
            delay(pause);
        }
        
        LOG_NOTICE( "voltage=", data.voltage);

		Serial << "\n";
        Serial << "version=" << data.version << " service=" << data.service << " diag=" << data.diagnostic << "\n";
        Serial << "data=" << data.value0 << " " << data.value1 << "\n";

        LOG_NOTICE( "ESP", "Going to sleep" );

        masterI2C.sendCmd( 'Z' );	// Tell slave we are going to sleep
       
        while (digitalRead(DETECT_PIN) == HIGH) 
            ; //wait turn off

        LOG_NOTICE( "ESP", "Power OFF" );
    } else {

        unsigned long now = millis();
        if (now - step > 1000) {
            sec++;
            step = now;

            if (sec % 2 > 0) {

                digitalWrite(OUTPUT_1, HIGH);
                digitalWrite(OUTPUT_2, HIGH);
            } else {

                digitalWrite(OUTPUT_1, LOW);
                digitalWrite(OUTPUT_2, LOW);
            }
        }

        if ((now - reset) / 1000 > 25 * 60) {
            reset = now;
            LOG_NOTICE( "ESP", "Reset!" );
            digitalWrite(RESET_PIN, LOW);
            delay(10);
            digitalWrite(RESET_PIN, HIGH);
        }
    }
}
