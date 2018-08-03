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
	uint8_t  version;     //Версия ПО Attiny
	uint8_t  service;     //Причина загрузки Attiny
	uint32_t voltage;     //Напряжение питания в мВ
	uint32_t impulses0;   //Импульсов, канал 0
	uint32_t impulses1;   //Импульсов, канал 1
	uint8_t  diagnostic;  //1 - ок, 0 - нет связи с Attiny
	uint8_t  reserved2;
}; //should be *16bit https://github.com/esp8266/Arduino/issues/1825


class MasterI2C
{
protected:
	bool getUint(uint32_t &value);
	bool getByte(uint8_t &value);
	uint8_t mode;

public:
	void begin();
	void end();
	bool sendCmd( const char cmd );
	bool getSlaveData(SlaveData &data);
	bool setup_mode();
};


#endif

