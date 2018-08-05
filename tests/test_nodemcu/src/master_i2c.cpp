#include "master_i2c.h"
#include "Logging.h"
#include <Wire.h>
#include <Arduino.h>
#include "setup.h"


void MasterI2C::begin() {
	Wire.begin( SDA_PIN, SCL_PIN );
	Wire.setClock( 100000L );
	Wire.setClockStretchLimit(1500L);

	mode = TRANSMIT_MODE;
	if (!sendCmd('M') || !getByte(mode)) {
		LOG_ERROR("I2C", "get mode failed. Check i2c line.");
	}
	else {
		LOG_NOTICE("I2C", "mode=" << mode);
	}
}


bool MasterI2C::sendCmd(const char cmd ) {
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
	
	delay(1); // Because attiny is running slow cpu clock speed. Give him a little time to process the command.
	return true;
}

bool MasterI2C::setup_mode() {
	//pinMode(D6, INPUT_PULLUP);
	//return digitalRead(D6) == LOW; //
	return mode == SETUP_MODE;
}

bool MasterI2C::getByte(uint8_t &value) {
	if (Wire.requestFrom( I2C_SLAVE_ADDR, 1 ) != 1)	{
		LOG_ERROR("I2C", "requestFrom failed");
		return false;
	}
	value = Wire.read();

#if LOGLEVEL >= 6
	Serial.print(value, HEX);
	Serial << " ";
#endif

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
	good &= getUint(data.impulses0);
	good &= getUint(data.impulses1);
	data.diagnostic = good;
	
	if (good) {
		LOG_NOTICE("I2C", "version: " << data.version);
		LOG_NOTICE("I2C", "service: " << data.service);
		LOG_NOTICE("I2C", "voltage: " << data.voltage);
		LOG_NOTICE("I2C", "impulses0: " << data.impulses0);
		LOG_NOTICE("I2C", "impulses1: " << data.impulses1);

	} else {
		LOG_ERROR("I2C", "data failed");
	}
	return good;
}


