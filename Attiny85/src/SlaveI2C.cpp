
#include "Setup.h"
#include "SlaveI2C.h"
#include <Arduino.h>
#include "Storage.h"
#include "Power.h"
#include <Wire.h>

extern struct Header info;
extern void saveConfig();
extern uint32_t wakeup_period;
extern void extendWakeUpPeriod();
extern void measureVoltage(uint16_t vcc_real_mv);


/* Static declaration */
uint8_t SlaveI2C::txBufferPos = 0;
uint8_t SlaveI2C::txBuffer[TX_BUFFER_SIZE];
uint8_t SlaveI2C::setup_mode = TRANSMIT_MODE;
bool SlaveI2C::masterSentSleep = false;

void SlaveI2C::begin(const uint8_t mode)
{

    setup_mode = mode;
    Wire.begin(I2C_SLAVE_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);   
    masterSentSleep = false;
    newCommand();
}

void SlaveI2C::end()
{
    Wire.end();
}

void SlaveI2C::requestEvent()
{
    Wire.write(txBuffer[txBufferPos]);
    if (txBufferPos + 1 < TX_BUFFER_SIZE)
        txBufferPos++; // Avoid buffer overrun if master misbehaves
}

void SlaveI2C::newCommand()
{
    memset(txBuffer, 0xAA, TX_BUFFER_SIZE); // Zero the tx buffer (with 0xAA so master has a chance to see he is stupid)
    txBufferPos = 0;                        // The next read from master starts from begining of buffer
}

/* Depending on the received command from master, set up the content of the txbuffer so he can get his data */

void SlaveI2C::receiveEvent(int howMany)
{
    static uint8_t command;

    command = Wire.read(); // Get instructions from master

    newCommand();
    switch (command)
    {
    case 'B': // данные
        info.crc = crc_8((unsigned char *)&info, HEADER_DATA_SIZE);
        memcpy(txBuffer, &info, TX_BUFFER_SIZE);
        break;
    case 'Z': // Готовы ко сну
        masterSentSleep = true;
        break;
    case 'M': // Разбудили ESP для настройки или передачи данных?
        txBuffer[0] = setup_mode;
        break;
    case 'T': // После настройки ESP сменит режим пробуждения и сразу скинет данные
              // MANUAL потому что при TRANSMIT_MODE ESP корректирует время
        setup_mode = MANUAL_TRANSMIT_MODE;
        break;
    case 'S': // ESP присылает новое значение периода пробуждения
        getWakeUpPeriod();
        break;
    case 'E': // ESP продлевает время пробуждения
        extendWakeUp();
        break;
    case 'C': // ESP присылает новую конфигурацию
        getCounterTypes();
        break;
    case 'V': // ESP запрос на измерение напряжения =0 или каллибровку attiny
        getVoltage();
        break;
    }
}

uint16_t SlaveI2C::getUint16()
{
    uint8_t data[2];

    data[0] = Wire.read();
    data[1] = Wire.read();
    uint8_t crc = Wire.read();

    uint16_t value = (data[0] << 8) | data[1];

    if (crc == crc_8(data, 2))
        return value;
    return 0;
}

void SlaveI2C::getWakeUpPeriod()
{
    uint16_t newPeriod = SlaveI2C::getUint16();
    if (newPeriod != 0)
    {
        wakeup_period = ONE_MINUTE * newPeriod;
    }
}

void SlaveI2C::getCounterTypes()
{
    uint8_t data[sizeof(CounterTypes)];

    for (uint8_t i=0; i < sizeof(CounterTypes); i++)
    {
        data[i] = Wire.read();
    }
    uint8_t crc = Wire.read();

    if (crc == crc_8(data, sizeof(CounterTypes))) 
    {
        memcpy((void*)&(info.config.types), data, sizeof(CounterTypes));
        saveConfig();
    }
}

bool SlaveI2C::masterGoingToSleep()
{
    return masterSentSleep;
}

void SlaveI2C::extendWakeUp()
{
    extendWakeUpPeriod();
}

void SlaveI2C::getVoltage()
{
    uint16_t vcc_real_mv = SlaveI2C::getUint16();
    measureVoltage(vcc_real_mv);
}