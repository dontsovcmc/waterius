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
bool MasterI2C::getByte(uint8_t *value, uint8_t &crc)
{
    if (Wire.requestFrom(I2C_SLAVE_ADDR, 1) != 1)
    {
        LOG_ERROR(F("RequestFrom failed"));
        return false;
    }
    *value = Wire.read();
    crc = crc_8(value, 1, crc);
    return true;
}

/**
 * @brief Чтение заданного количества байт в буффер по указателю
 * 
 * @param value указатель на буфер приемника
 * @param count количество байт для чтения
 * @param crc контрольная сумма для расчета
 * @return true прочитано успешно
 * @return false данные не прочитаны
 */
bool MasterI2C::getBytes(uint8_t *value, uint8_t count, uint8_t &crc)
{
    for (; count > 0; count--, value++)
    {
        if (!getByte(value, crc))
            return false;
    }
    return true;
}


bool MasterI2C::getMode(uint8_t *mode)
{
    uint8_t crc = init_crc;
    *mode = TRANSMIT_MODE;
    if (!sendCmd('M') || !getByte(mode, crc))
    {
        LOG_ERROR(F("Attiny not found."));
        return false;
    }
    LOG_INFO("mode: " << *mode);
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
    uint8_t crc = 0xFF;

    if (!getByte((uint8_t *)&data.version, crc))
    {
        LOG_ERROR(F("Data failed"));
        return false;
    }
     
    if (data.version < 33) 
    {
        AttinyData32ver data32;
        data32.version = data.version;
        if (!getBytes((uint8_t *)&data32.service, sizeof(AttinyData32ver) - 1, crc)) // -1 т.к. version уже прочитали
        {
            LOG_ERROR(F("Data failed"));
            return false;
        }
        data.service = data32.service;
        data.setup_started_counter = data32.setup_started_counter;
        data.resets = data32.resets;
        data.counter_type0 = data32.counter_type0;
        data.counter_type1 = data32.counter_type1;
        data.impulses0 = data32.impulses0;
        data.impulses1 = data32.impulses1;
        data.adc0 = data32.adc0;
        data.adc1 = data32.adc1;
        data.crc = data32.crc;
    }

    if (!getBytes((uint8_t *)&data.service, sizeof(AttinyData) - 1, crc))
    {
        LOG_ERROR(F("Data failed"));
        return false;
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

    if (data.version < 29)
    {
        LOG_ERROR(F("ATTINY: unsupported firmware ver.") << data.version);
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
    txBuf[3] = crc_8(&txBuf[1], 2, init_crc);

    if (!sendData(txBuf, 4))
    {
        return false;
    }
    return true;
}