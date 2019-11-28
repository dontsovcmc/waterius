#include "master_i2c.h"
#include "Logging.h"
#include <Wire.h>
#include <Arduino.h>
#include "setup.h"


void MasterI2C::begin() {

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000L);
    Wire.setClockStretchLimit(1500L);  // Иначе связь с Attiny не надежная будут FF FF в хвосте посылки 
}


bool MasterI2C::sendCmd(const char cmd ) {

    Wire.beginTransmission( I2C_SLAVE_ADDR );
    if (Wire.write(cmd) != 1){
        LOG_ERROR(FPSTR(S_I2C), F("Write cmd failed"));
        return false;
    }
    int err = Wire.endTransmission(true);
    if (err != 0) {
        LOG_ERROR(FPSTR(S_I2C), "end error:" << err);
        return false;
    }    
    
    delay(1); // Дадим Attiny время подумать 
    return true;
}

bool MasterI2C::getByte(uint8_t &value) {

    if (Wire.requestFrom( I2C_SLAVE_ADDR, 1 ) != 1) {
        LOG_ERROR(FPSTR(S_I2C), F("RequestFrom failed"));
        return false;
    }
    value = Wire.read();

#if LOGLEVEL >= 6
    Serial.print(value, HEX);
    Serial << " ";
#endif

    return true;
}

bool MasterI2C::getUint(uint32_t &value) {

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

bool MasterI2C::getMode(uint8_t &mode) {

    mode = TRANSMIT_MODE;
    if (!sendCmd('M') || !getByte(mode)) {
        LOG_ERROR(FPSTR(S_I2C), F("GetMode failed. Check i2c line."));
        return false;
    }
    LOG_INFO(FPSTR(S_I2C), "mode=" << mode);
    return true;
}

bool MasterI2C::getSlaveData(SlaveData &data) {

    sendCmd('B');
    uint8_t dummy;
    bool good = getByte(data.version);
    good &= getByte(data.service);
    good &= getUint(data.voltage);

    if (data.version >= 5) {
        good &= getByte(data.resets);
        good &= getByte(dummy);
    }
    if (data.version >= 8) {
        good &= getByte(data.state0);
        good &= getByte(data.state1);
    }
    good &= getUint(data.impulses0);
    good &= getUint(data.impulses1);
    data.diagnostic = good;
    
    if (good) {
        LOG_INFO(FPSTR(S_I2C), F("version: ") << data.version);
        LOG_INFO(FPSTR(S_I2C), F("service: ") << data.service);
        LOG_INFO(FPSTR(S_I2C), F("voltage: ") << data.voltage);
        LOG_INFO(FPSTR(S_I2C), F("resets: ") << data.resets);
        LOG_INFO(FPSTR(S_I2C), F("state0: ") << data.state0);
        LOG_INFO(FPSTR(S_I2C), F("state1: ") << data.state1);
        LOG_INFO(FPSTR(S_I2C), F("impulses0: ") << data.impulses0);
        LOG_INFO(FPSTR(S_I2C), F("impulses1: ") << data.impulses1);

    } else {
        LOG_ERROR(FPSTR(S_I2C), F("Data failed"));
    }
    return good;
}


