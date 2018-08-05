#ifndef _MASTERI2C_h
#define _MASTERI2C_h

#include <Arduino.h>

/*
	В зависимости от модификации ESP8266 может быть разным
*/
#define SDA_PIN 0
#define SCL_PIN 2

//attiny85
#define SETUP_MODE 1
#define TRANSMIT_MODE 2

#define I2C_SLAVE_ADDR 10


struct SlaveData {
	uint8_t  version;
	uint8_t  service;
	uint32_t voltage;
	uint32_t impulses0;
	uint32_t impulses1;
	uint8_t  diagnostic;  //1 - good, 0 - fail connect with attiny
	uint8_t  reserved2;
}; //should be *16bit https://github.com/esp8266/Arduino/issues/1825


class MasterI2C
{
protected:
	bool getUint(uint32_t &value);
	bool getByte(uint8_t &value);

public:
	uint8_t mode;
	void begin();
	void end();
	bool sendCmd( const char cmd );
	bool getSlaveData(SlaveData &data);
	bool setup_mode();
};


#endif

