#ifndef _SLAVEI2C_h
#define _SLAVEI2C_h

#include <Arduino.h>

#define I2C_SLAVE_ADDRESS 10
#define TX_BUFFER_SIZE 20

#define SETUP_MODE 1
#define TRANSMIT_MODE 2

class SlaveI2C
{
 protected:
	 static byte txBuffer[TX_BUFFER_SIZE];
	 static uint8_t txBufferPos;
	 static bool setup_mode;

	 static char lastCommand;
	 static bool masterSentSleep;
	 static bool masterAckOurData;
	 static bool masterCheckMode;

	 static void requestEvent();
	 static void newCommand();
	 static void receiveEvent( int howMany );

 public:
	 void begin(const bool);
	 static void end();
	 bool masterGoingToSleep();
	 bool masterGotOurData();
	 bool masterModeChecked();
};



#endif

