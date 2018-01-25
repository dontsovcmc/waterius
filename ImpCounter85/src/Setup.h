#ifndef _SETUP_h
#define _SETUP_h

enum State { // Our state machine
		SLEEP,
		MEASURING,
		MASTER_WAKE,
		SENDING
};

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

const uint8_t DEVICE_ID = 1;                // Unique identifier of this device
const uint8_t NUMBER_OF_SENSORS = 2;		// How many sensors deliver data

const uint8_t GIVEUP_ON_MASTER_AFTER = 3;	// If master havn't confirmed getting our data within XX seconds, we give up and continue measuring

const uint32_t WAKE_MASTER_EVERY = 10;	// Every XX seconds we wake up ESP master for it to poll our data
const uint32_t MEASUREMENT_EVERY = 3;		// How often we take a measurement from our sensors

#define STORAGE_SIZE 60  //bytes, 8 byte 1 measure


#endif

