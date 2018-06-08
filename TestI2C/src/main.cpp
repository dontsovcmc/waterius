#include "MasterI2C.h"
#include "Logging.h"

#define LOGLEVEL 6

#define I2C_SLAVE_ADDR 10

#define DETECT_PIN 9
#define OUTPUT_1 A0
#define OUTPUT_2 A1

MasterI2C masterI2C;


void setup()
{
	pinMode(DETECT_PIN, INPUT);
	pinMode(OUTPUT_1, OUTPUT);
	pinMode(OUTPUT_2, OUTPUT);
    
	Serial.begin(115200);
    
	LOG_NOTICE( "ESP", "Booted" );
}


void loop()
{
    static unsigned long i=0;
    static unsigned long sec=0;
    static unsigned long st=0;

    //attiny подало питание на ESP
    if (digitalRead(DETECT_PIN) == HIGH)
    {
		Serial << "\n";
        LOG_NOTICE( "ESP", "powe on I2C-begined" );

        masterI2C.begin();
        if (masterI2C.setup_mode())
            LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
        else
            LOG_NOTICE( "ESP", "I2C-begined: mode TRANSMIT" );

        SlaveData data;
        masterI2C.getSlaveData(data);

        LOG_NOTICE( "voltage=", data.voltage);

		Serial << "\n";
        Serial << "version=" << data.version << " service=" << data.service << " diag=" << data.diagnostic << "\n";
        Serial << "data=" << data.value0 << " " << data.value1 << "\n";

        LOG_NOTICE( "ESP", "Going to sleep" );

        masterI2C.sendCmd( 'Z' );	// Tell slave we are going to sleep
       
        while (digitalRead(DETECT_PIN) == HIGH) 
            ; //wait turn off

        LOG_NOTICE( "ESP", "Power OFF" );
    }
    else
    {
        unsigned long now = millis();
        if (now - st > 250)
        {
            st = now;
            Serial.print('.');
            Serial.print(sec++);

            if (sec % 2)
            {
                digitalWrite(OUTPUT_1, LOW);
                digitalWrite(OUTPUT_2, HIGH);
            }
            else
            {
                digitalWrite(OUTPUT_1, HIGH);
                digitalWrite(OUTPUT_2, LOW);
            }
        }
    }
}
