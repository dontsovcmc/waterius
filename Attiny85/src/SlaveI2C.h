#ifndef _SLAVEI2C_h
#define _SLAVEI2C_h

#include <Arduino.h>

#define I2C_SLAVE_ADDRESS 10

#define SETUP_MODE 1
#define TRANSMIT_MODE 2
#define MANUAL_TRANSMIT_MODE 3

class SlaveI2C
{
protected:
    static uint8_t txBuffer[TX_BUFFER_SIZE];
    static uint8_t txBufferPos;
    static uint8_t setup_mode;

    static bool masterSentSleep;

    static void requestEvent();
    static void newCommand();
    static void receiveEvent(int howMany);
    static void getWakeUpPeriod();	
    static void getCounterValue();

public:
    void begin(const uint8_t);
    static void end();
    bool masterGoingToSleep();
};

union ArrayToUin32
{
	byte array[4];
	uint32_t value;
};

#endif
