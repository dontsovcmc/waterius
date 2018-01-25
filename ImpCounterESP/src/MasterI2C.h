#ifndef _MASTERI2C_h
#define _MASTERI2C_h

#include <Arduino.h>

#include "setup.h"

struct SlaveStats {
	uint32_t bytesReady;
	uint32_t masterWakeEvery;
	uint32_t measurementEvery;
	uint8_t bytesPerMeasurement;
	uint32_t voltage;
	uint8_t deviceID;
	uint8_t numberOfSensors;
};


class MasterI2C
{
 protected:
	 void gotoFirstByte();
	 byte getNextByte();
	 uint32_t getUint();
	 uint8_t getByte();

 public:
	 void begin();
	 void sendCmd( const char cmd );
	 uint16_t getSlaveStorage( byte* storage, const uint16_t maxStorageSize, const uint16_t bytesToFetch );
	 SlaveStats getSlaveStats();
};


#endif

