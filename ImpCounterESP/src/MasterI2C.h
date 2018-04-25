#ifndef _MASTERI2C_h
#define _MASTERI2C_h

#include <Arduino.h>

#define I2C_SLAVE_ADDR 10

/*
d.bytes, d.version, d.dymmy, d.wake, d.period, d.voltage = struct.unpack('HBBHHH', data[0:10])
d.device_id, d.device_pwd = struct.unpack('HH', data[10:14])
*/
struct SlaveStats {
	uint16_t bytesReady;
	uint8_t  version;
	uint8_t  message_type;
	uint16_t masterWakeEvery;
	uint16_t measurementEvery;
	uint16_t vcc;
	uint16_t deviceID;
	uint16_t devicePWD;
}; //should be *16bit https://github.com/esp8266/Arduino/issues/1825


enum msg_type {
	ATTINY_OK = 1,
	ATTINY_FAIL = 2
};

class MasterI2C
{
 protected:
	 void gotoFirstByte();
	 byte getNextByte();
	 uint16_t getUint();
	 uint8_t getByte();

 public:
	 void begin();
	 void end();
	 void sendCmd( const char cmd );
	 uint16_t getSlaveStorage( byte* storage, const uint16_t maxStorageSize, const uint16_t bytesToFetch );
	 SlaveStats getSlaveStats();
};


#endif

