#include "MasterI2C.h"
#include "Logging.h"
#include <Arduino.h>

#include "setup.h"

#define SOFT_I2C

#ifdef SOFT_I2C
	#define SDA_PORT PORTB
	#define SDA_PIN 5
	#define SCL_PORT PORTB
	#define SCL_PIN 3

	#include <SoftI2CMaster.h>
#else
	#include <Wire.h>
#endif


void MasterI2C::begin() {

#ifndef SOFT_I2C
	Wire.begin();
	Wire.setClock( 100000L );
#else
	if (!i2c_init())
		LOG_ERROR("I2C", "I2C init failed");
#endif 

	//Wire.setClockStretchLimit(1500L); //нет в atmega

	mode = TRANSMIT_MODE;
	if (!sendCmd('M') || !getByte(mode)) {
		LOG_ERROR("I2C", "get mode failed. Check i2c line.");
	}
}

bool MasterI2C::sendCmd(const char cmd ) {

#ifndef SOFT_I2C
	Wire.beginTransmission( I2C_SLAVE_ADDR );
	if (Wire.write(cmd) != 1){
		LOG_ERROR("I2C", "write cmd failed");
		return false;
	}
	int err = Wire.endTransmission(true);
	if (err != 0) {
		LOG_ERROR("I2C end", err);
		return false;
	}
#else
	i2c_rep_start((I2C_SLAVE_ADDR<<1)|I2C_WRITE);
	i2c_write(cmd);
#endif
	
	delay(1); // Because attiny is running slow cpu clock speed. Give him a little time to process the command.
	return true;
}

bool MasterI2C::setup_mode() {
	return mode == SETUP_MODE;
}

bool MasterI2C::getByte(uint8_t &value) {
#ifndef SOFT_I2C
	if (Wire.requestFrom( I2C_SLAVE_ADDR, 1 ) != 1)	{
		LOG_ERROR("I2C", "requestFrom failed");
		return false;
	}
	value = Wire.read();
#else
	i2c_rep_start((I2C_SLAVE_ADDR<<1)|I2C_READ);
	value = i2c_read(true);
	i2c_stop();
#endif
	Serial.print(value, HEX);
	Serial << " ";
	return true;
}

bool MasterI2C::getUint(uint32_t &value) 
{
	uint8_t i1, i2, i3, i4;
	if (getByte(i1) && getByte(i2) && getByte(i3) && getByte(i4)) {
		value = i4;
		value = value << 8;
		value |= i3;
		value = value << 8;
		value |= i2;
		value = value << 8;
		value |= i1;
		//вот так не работает из-за преобразования типов:
		//value = i1 | (i2 << 8) | (i3 << 16) | (i4 << 24);
		return true;
	}
	return false;
}


bool MasterI2C::getSlaveData(SlaveData &data)
{
	sendCmd( 'B' );
	bool good = getByte(data.version);
	good &= getByte(data.service);
	good &= getUint(data.voltage);
	good &= getUint(data.value0);
	good &= getUint(data.value1);
	data.diagnostic = good;
	
	return good;
}