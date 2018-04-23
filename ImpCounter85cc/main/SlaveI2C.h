#ifndef _SLAVEI2C_h
#define _SLAVEI2C_h

#include <Arduino.h>

#define I2C_SLAVE_ADDRESS 10
#define TX_BUFFER_SIZE 20


class SlaveI2C
{
 protected:
	 static byte txBuffer[TX_BUFFER_SIZE];
	 static uint8_t txBufferPos;

	 static char lastCommand;
	 static bool masterSentSleep;
	 static bool masterAckOurData;

	 static void requestEvent();
	 static void newCommand();
	 static void receiveEvent( int howMany );

 public:
	 void begin();
	 static void end();
	 bool masterGoingToSleep();
	 bool masterGotOurData();
};



#endif

