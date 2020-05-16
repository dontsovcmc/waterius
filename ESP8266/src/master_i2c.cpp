#include "master_i2c.h"
#include "Logging.h"
#include <Wire.h>
#include <Arduino.h>
#include "setup.h"

//https://github.com/lammertb/libcrc/blob/600316a01924fd5bb9d47d535db5b8f3987db178/src/crc8.c

static uint8_t sht75_crc_table[] = {

	0,   49,  98,  83,  196, 245, 166, 151, 185, 136, 219, 234, 125, 76,  31,  46,
	67,  114, 33,  16,  135, 182, 229, 212, 250, 203, 152, 169, 62,  15,  92,  109,
	134, 183, 228, 213, 66,  115, 32,  17,  63,  14,  93,  108, 251, 202, 153, 168,
	197, 244, 167, 150, 1,   48,  99,  82,  124, 77,  30,  47,  184, 137, 218, 235,
	61,  12,  95,  110, 249, 200, 155, 170, 132, 181, 230, 215, 64,  113, 34,  19,
	126, 79,  28,  45,  186, 139, 216, 233, 199, 246, 165, 148, 3,   50,  97,  80,
	187, 138, 217, 232, 127, 78,  29,  44,  2,   51,  96,  81,  198, 247, 164, 149,
	248, 201, 154, 171, 60,  13,  94,  111, 65,  112, 35,  18,  133, 180, 231, 214,
	122, 75,  24,  41,  190, 143, 220, 237, 195, 242, 161, 144, 7,   54,  101, 84,
	57,  8,   91,  106, 253, 204, 159, 174, 128, 177, 226, 211, 68,  117, 38,  23,
	252, 205, 158, 175, 56,  9,   90,  107, 69,  116, 39,  22,  129, 176, 227, 210,
	191, 142, 221, 236, 123, 74,  25,  40,  6,   55,  100, 85,  194, 243, 160, 145,
	71,  118, 37,  20,  131, 178, 225, 208, 254, 207, 156, 173, 58,  11,  88,  105,
	4,   53,  102, 87,  192, 241, 162, 147, 189, 140, 223, 238, 121, 72,  27,  42,
	193, 240, 163, 146, 5,   52,  103, 86,  120, 73,  26,  43,  188, 141, 222, 239,
	130, 179, 224, 209, 70,  119, 36,  21,  59,  10,  89,  104, 255, 206, 157, 172
};

uint8_t crc_8(unsigned char *input_str, size_t num_bytes, uint8_t crc) {

	unsigned char *ptr = input_str;

	if (ptr != NULL) {
		for (size_t a = 0; a < num_bytes; a++) {
			crc = sht75_crc_table[(*ptr) ^ crc];
            ptr++;
		}
	}

	return crc;
}


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

bool MasterI2C::getByte(uint8_t &value, uint8_t &crc) {

    if (Wire.requestFrom( I2C_SLAVE_ADDR, 1 ) != 1) {
        LOG_ERROR(FPSTR(S_I2C), F("RequestFrom failed"));
        return false;
    }
    value = Wire.read();
    crc = crc_8(&value, 1, crc);
    return true;
}

bool MasterI2C::getUint16(uint16_t &value, uint8_t &crc) {

    uint8_t i1, i2;
    if (getByte(i1, crc) && getByte(i2, crc)) {
        value = i2;
        value = value << 8;
        value |= i1;
        return true;
    }
    return false;
}

bool MasterI2C::getUint(uint32_t &value, uint8_t &crc) {

    uint8_t i1, i2, i3, i4;
    if (getByte(i1, crc) && getByte(i2, crc) && getByte(i3, crc) && getByte(i4, crc)) {
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

    uint8_t crc; //not used
    mode = TRANSMIT_MODE;
    if (!sendCmd('M') || !getByte(mode, crc)) {
        LOG_ERROR(FPSTR(S_I2C), F("GetMode failed. Check i2c line."));
        return false;
    }
    LOG_INFO(FPSTR(S_I2C), "mode: " << mode);
    return true;
}

bool MasterI2C::getSlaveData(SlaveData &data) {

    sendCmd('B');

    data.diagnostic = WATERIUS_NO_LINK;

    uint8_t dummy, crc = 0;
    bool good = getByte(data.version, crc);
    good &= getByte(data.service, crc);
    good &= getUint(data.voltage, crc);

    if (data.version >= 5) {
        good &= getByte(data.resets, crc);
        good &= getByte(data.reserved, crc);
    }
    if (data.version >= 8) {
        good &= getByte(data.state0, crc);
        good &= getByte(data.state1, crc);
    }
    good &= getUint(data.impulses0, crc);
    good &= getUint(data.impulses1, crc);

    if (data.version < 12) {
        data.diagnostic = good ? WATERIUS_OK : WATERIUS_NO_LINK;
    }
    else {
        good &= getUint16(data.adc0, crc);
        good &= getUint16(data.adc1, crc);

        good &= getByte(data.crc, dummy);

        if (good) {
            data.diagnostic = (data.crc == crc) ? WATERIUS_OK : WATERIUS_BAD_CRC;
        }
    } 

    switch (data.diagnostic) {
        case WATERIUS_BAD_CRC:
            LOG_ERROR(FPSTR(S_I2C), F("CRC wrong"));
        case WATERIUS_OK:
            LOG_INFO(FPSTR(S_I2C), F("version: ") << data.version);
            LOG_INFO(FPSTR(S_I2C), F("service: ") << data.service);
            LOG_INFO(FPSTR(S_I2C), F("voltage: ") << data.voltage);
            LOG_INFO(FPSTR(S_I2C), F("resets: ") << data.resets);
            LOG_INFO(FPSTR(S_I2C), F("state0: ") << data.state0);
            LOG_INFO(FPSTR(S_I2C), F("state1: ") << data.state1);
            LOG_INFO(FPSTR(S_I2C), F("impulses0: ") << data.impulses0);
            LOG_INFO(FPSTR(S_I2C), F("impulses1: ") << data.impulses1);
            LOG_INFO(FPSTR(S_I2C), F("adc0: ") << data.adc0);
            LOG_INFO(FPSTR(S_I2C), F("adc1: ") << data.adc1);
            LOG_INFO(FPSTR(S_I2C), F("CRC ok"));
        break;
        case WATERIUS_NO_LINK:
            LOG_ERROR(FPSTR(S_I2C), F("Data failed"));
    };

    return data.diagnostic == WATERIUS_OK;
}


