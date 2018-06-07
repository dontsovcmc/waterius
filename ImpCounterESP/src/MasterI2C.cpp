#include "MasterI2C.h"
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
	pinMode(D6, INPUT_PULLUP);
	return digitalRead(D6) == LOW; //mode == SETUP_MODE;
}

bool MasterI2C::getByte(uint8_t &value) {
	if (Wire.requestFrom( I2C_SLAVE_ADDR, 1 ) != 1)	{
		LOG_ERROR("I2C", "requestFrom failed");
		return false;
	}
	value = Wire.read();
	return true;
}

bool MasterI2C::getUint(uint16_t &value) 
{
	uint8_t i1, i2;
	if (!getByte(i1) || !getByte(i2)) {
		return false;
	}
	value = i1 | (i2 << 8);
	return true;
}

bool MasterI2C::getSlaveData(SlaveData &data)
{
	sendCmd( 'B' );
	bool good = getByte(data.version);
	good &= getByte(data.service);
	good &= getUint(data.voltage);
	good &= getUint(data.value1);
	good &= getUint(data.value2);
	data.diagnostic = good;
	
	return good;
}


