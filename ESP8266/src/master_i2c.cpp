#include "master_i2c.h"
#include "Logging.h"
#include <Wire.h>
#include <Arduino.h>
#include "setup.h"

// https://github.com/lammertb/libcrc/blob/600316a01924fd5bb9d47d535db5b8f3987db178/src/crc8.c

// https://gist.github.com/brimston3/83cdeda8f7d2cf55717b83f0d32f9b5e
// https://www.onlinegdb.com/online_c++_compiler
// Dallas CRC x8+x5+x4+1
uint8_t crc_8(unsigned char *b, size_t num_bytes, uint8_t crc)
{
    while (num_bytes--)
    {
        uint8_t inbyte = *b++;
        for (uint8_t i = 8; i; i--)
        {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix)
                crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

void MasterI2C::begin()
{
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000L);
    Wire.setClockStretchLimit(2500L); // Иначе связь с Attiny не надежная будут FF FF в хвосте посылки
}

bool MasterI2C::sendCmd(uint8_t cmd)
{
    return sendData(&cmd, 1);
}

bool MasterI2C::sendData(uint8_t *buf, size_t size)
{
    uint8_t i;
    Wire.beginTransmission(I2C_SLAVE_ADDR);
    for (i = 0; i < size; i++)
    {
        if (Wire.write(buf[i]) != 1)
        {
            LOG_ERROR(F("I2C transmitting fail."));
            return false;
        }
    }
    int err = Wire.endTransmission(true);
    if (err != 0)
    {
        LOG_ERROR(F("end error:") << err);
        return false;
    }

    return true;
}

/**
 * @brief Чтение одного байта по указателю
 * 
 * @param value указатель приемника
 * @param crc контрольная сумма
 * @return true прочитано успешно
 * @return false данные не прочитаны
 */
bool MasterI2C::getByte(uint8_t &value, uint8_t &crc)
{
    if (Wire.requestFrom(I2C_SLAVE_ADDR, 1) != 1)
    {
        LOG_ERROR(F("RequestFrom failed"));
        return false;
    }
    value = Wire.read();
    crc = crc_8(&value, 1, crc);
    return true;
}

bool MasterI2C::getUint16(uint16_t &value, uint8_t &crc)
{
    uint8_t i1, i2;
    if (getByte(i1, crc) && getByte(i2, crc))
    {
        value = value << 8;
        value |= i2;
        value = value << 8;
        value |= i1;
        // вот так не работает из-за преобразования типов:
        //  value = i1 | (i2 << 8) | (i3 << 16) | (i4 << 24);
        return true;
    }
    return false;
}

bool MasterI2C::getUint32(uint32_t &value, uint8_t &crc)
{

    uint8_t i1, i2, i3, i4;
    if (getByte(i1, crc) && getByte(i2, crc) && getByte(i3, crc) && getByte(i4, crc))
    {
        value = i4;
        value = value << 8;
        value |= i3;
        value = value << 8;
        value |= i2;
        value = value << 8;
        value |= i1;
        // вот так не работает из-за преобразования типов:
        //  value = i1 | (i2 << 8) | (i3 << 16) | (i4 << 24);
        return true;
    }
    return false;
}


bool MasterI2C::getMode(uint8_t &mode)
{
    uint8_t crc = INIT_ATTINY_CRC;
    mode = TRANSMIT_MODE;
    if (!sendCmd('M') || !getByte(mode, crc))
    {
        LOG_ERROR(F("Attiny not found."));
        return false;
    }
    LOG_INFO("mode: " << mode);
    return true;
}

/**
 * @brief Чтение данных с прибора.
 * 
 * Данные читаются в буффер, до контрольной суммы включительно. 
 * При успешном чтении расчитанная контрольная сумма должна быть равно 0, 
 * в противном случае во время чтения произошла ошибка.
 * 
 * @param data Структура для заполнения данными
 * @return true прочитанно успешно.
 * @return false произошла ошибка
 */
bool MasterI2C::getSlaveData(AttinyData &data)
{
    sendCmd('B');
    uint8_t crc = INIT_ATTINY_CRC;

    bool good = getByte(data.version, crc);
    if (!good)
    {
        LOG_ERROR(F("Data failed"));
        return false;
    }

    if (data.version < 29)
    {
        LOG_ERROR(F("ATTINY: unsupported firmware ver.") << data.version);
        return false;
    }

    if (data.version < 33) 
    {   
        uint8_t reserved8;
        uint16_t reserved16;

        good &= getByte(data.service, crc);
        good &= getUint16(reserved16, crc);
        good &= getByte(reserved8, crc);
        good &= getByte(data.setup_started_counter, crc);

        good &= getByte(data.resets, crc);
        good &= getByte(reserved8, crc);
        good &= getByte(data.counter_type0, crc);
        good &= getByte(data.counter_type1, crc);

        good &= getUint32(data.impulses0, crc);
        good &= getUint32(data.impulses1, crc);

        good &= getUint16(data.adc0, crc);
        good &= getUint16(data.adc1, crc);

        good &= getByte(data.crc, crc);
    }
    else
    {
        good &= getByte(data.service, crc);
        good &= getUint16(data.voltage, crc);
        good &= getByte(data.setup_started_counter, crc);

        good &= getByte(data.resets, crc);
        good &= getByte(data.counter_type0, crc);
        good &= getByte(data.counter_type1, crc);
        good &= getUint16(data.v_reference, crc);

        good &= getUint32(data.impulses0, crc);
        good &= getUint32(data.impulses1, crc);

        good &= getUint16(data.adc0, crc);
        good &= getUint16(data.adc1, crc);

        good &= getByte(data.crc, crc);
    }

    LOG_INFO(F("v") << data.version  << F(" service:") << data.service
        << F(" setups:") << data.setup_started_counter 
        << F(" resets:") << data.resets  
        << F(" voltage:") << data.voltage  << F(" v_ref:") << data.v_reference);

    LOG_INFO(F(" ctype0:") << data.counter_type0  << F(" imp0:") << data.impulses0 << F(" adc0:") << data.adc0);
    LOG_INFO(F(" ctype1:") << data.counter_type1  << F(" imp1:") << data.impulses1 << F(" adc1:") << data.adc1);

    if (crc)  // должна быть равна 0 
    {
        LOG_INFO(F("data.crc:") << data.crc  << F(" crc:") << crc);
        LOG_ERROR(F("!!! CRC wrong !!!!, go to sleep"));
        return false;
    }

    return true;
}

bool MasterI2C::setWakeUpPeriod(uint16_t period)
{
    uint8_t txBuf[4];

    txBuf[0] = 'S';
    txBuf[1] = (uint8_t)(period >> 8);
    txBuf[2] = (uint8_t)(period);
    txBuf[3] = crc_8(&txBuf[1], 2, 0xff);

    if (!sendData(txBuf, 4))
    {
        return false;
    }
    return true;
}

bool MasterI2C::setCountersType(const uint8_t type0, const uint8_t type1)
{
    uint8_t txBuf[4];

    txBuf[0] = 'C';
    txBuf[1] = type0;
    txBuf[2] = type1;
    txBuf[3] = crc_8(&txBuf[1], 2, INIT_ATTINY_CRC);

    if (!sendData(txBuf, 4))
    {
        return false;
    }
    return true;
}


bool MasterI2C::setReferenceVoltage(uint16_t voltage)
{
    uint8_t txBuf[4];

    txBuf[0] = 'V';
    txBuf[1] = (uint8_t)(voltage >> 8);
    txBuf[2] = (uint8_t)(voltage);
    txBuf[3] = crc_8(&txBuf[1], 2, 0xff);

    if (!sendData(txBuf, 4))
    {
        return false;
    }
    return true;
}
