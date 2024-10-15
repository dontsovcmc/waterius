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
#ifdef ESP8266
    Wire.setClockStretchLimit(2500L); // Иначе связь с Attiny не надежная будут FF FF в хвосте посылки
#endif
#ifdef ESP32
    Wire.setTimeout(2500L);
#endif
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
        value = i2;
        value = value << 8;
        value |= i1;
        return true;
    }
    return false;
}

bool MasterI2C::getUint(uint32_t &value, uint8_t &crc)
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
    uint8_t crc = init_crc;
    mode = TRANSMIT_MODE;
    if (!sendCmd('M') || !getByte(mode, crc))
    {
        LOG_ERROR(F("GetMode failed. Check i2c line."));
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
bool MasterI2C::getSlaveData(SlaveData &data)
{
    sendCmd('B');

    uint8_t dummy, crc = init_crc;
    bool good = getByte(data.version, crc);

    if (data.version < 29)
    {
        init_crc = 0; // в версиях <29 инициализация идет нулём
        crc = 0;
        crc = crc_8(&data.version, 1, crc);
    }

    good &= getByte(data.service, crc);
    good &= getUint16(data.reserved4, crc);
    good &= getByte(data.reserved, crc);
    good &= getByte(data.setup_started_counter, crc);

    good &= getByte(data.resets, crc);
    good &= getByte(data.model, crc);
    good &= getByte(data.counter_type0, crc);
    good &= getByte(data.counter_type1, crc);

    good &= getUint(data.impulses0, crc);
    good &= getUint(data.impulses1, crc);

    good &= getUint16(data.adc0, crc);
    good &= getUint16(data.adc1, crc);

    good &= getByte(data.crc, dummy);

    if (good)
    {
        if (data.crc != crc)
        {
            LOG_ERROR(F("!!! CRC wrong !!!!, go to sleep"));
        }
        else
        {
            LOG_INFO(F("v") << data.version << F(" setups:") << data.setup_started_counter << F(" resets:") << data.resets << F(" ctype0:") << data.counter_type0 << F(" ctype1:") << data.counter_type1 << F(" imp0:") << data.impulses0 << F(" imp1:") << data.impulses1 << F(" adc0:") << data.adc0 << F(" adc1:") << data.adc1);
            if (data.version >= 30)
                return true;

            LOG_ERROR(F("ATTINY: unsupported firmware ver.") << data.version);
        }
    }
    LOG_ERROR(F("Data failed"));
    return false;
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
    txBuf[3] = crc_8(&txBuf[1], 2, init_crc);

    if (!sendData(txBuf, 4))
    {
        return false;
    }
    return true;
}