
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
extern bool is_esp_powered_long();
extern uint8_t alarm_bits();
extern void set_alarm_config(const uint8_t *data);
extern void confirm_alarm();

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
        // Флаги собираем в момент запроса: главному циклу их вести незачем
        info.flags = (is_esp_powered_long() ? HEADER_FLAG_ESP_POWERED_LONG : 0) | alarm_bits();
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
    case 'P': // Ватериус-2: если юзер долго держит кнопку, то это режим настройки.
              // Передаем в attiny для случая нештатной перезагрузки ESP
        setup_mode = SETUP_MODE;
        break;
    case 'S': // ESP присылает новое значение периода пробуждения
        getWakeUpPeriod();
        break;
    case 'E': // ESP продлевает время бодрствования
        extendWakeUp();
        break;
    case 'C': // ESP присылает новую конфигурацию
        getCounterTypes();
        break;
    case 'A': // ESP присылает пороги тревог
        getAlarmConfig();
        break;
    case 'K': // ESP подтверждает, что доложила о тревоге получателю
        confirm_alarm();
        break;
    case 'V': // обновить напряжение
        info.voltage = readVcc();
        break;
    }
}

void SlaveI2C::getWakeUpPeriod()
{
    uint8_t data[2];

    data[0] = Wire.read();
    data[1] = Wire.read();
    uint8_t crc = Wire.read();

    uint16_t newPeriod = (data[0] << 8) | data[1];

    if ((crc == crc_8(data, 2)) && (newPeriod != 0))
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

/*
Пороги тревог: по два uint16 на канал, старшим байтом вперёд (issue #202).

Живут в ОЗУ и приезжают в каждом сеансе, как период пробуждения: EEPROM не
трогаем, иначе конфигурация у прошитых устройств не прошла бы проверку CRC.
Цена - после замены батареек тревоги молчат до первого сеанса ЕСП.
*/
void SlaveI2C::getAlarmConfig()
{
    uint8_t data[8];

    for (uint8_t i = 0; i < sizeof(data); i++)
    {
        data[i] = Wire.read();
    }
    uint8_t crc = Wire.read();

    if (crc == crc_8(data, sizeof(data)))
    {
        set_alarm_config(data);
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
