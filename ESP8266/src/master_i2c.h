#ifndef _MASTERI2C_h
#define _MASTERI2C_h

#include <Arduino.h>
#include "setup.h"

/*
Номера пинов линии i2c.
*/
#if WATERIUS_MODEL == WATERIUS_MODEL_1      // MODEL_CLASSIC (ESP-01)
#define SDA_PIN 0
#define SCL_PIN 2
#endif
#if WATERIUS_MODEL == WATERIUS_MODEL_2    // MODEL_2 (NodeMCU/ESP-12F)
#define SDA_PIN 4
#define SCL_PIN 5
#endif


#define INIT_ATTINY_CRC 0xFF

// Класс для синхронизации запросов к i2c, которые могут быть из разных потоков в AsyncWebServer
// В ESP отсутствуют мьютексы, поэтому будем считать, что это проще сдела
class BusyGuard {
    volatile bool& busy;
public:
    BusyGuard(volatile bool& flag) : busy(flag) {
        while (true) {
            noInterrupts();
            if (!busy) {
                busy = true;
                interrupts();
                break;
            }
            interrupts();
            yield();
        }
    }
    ~BusyGuard() {
        busy = false;
    }
    BusyGuard(const BusyGuard&) = delete;
    BusyGuard& operator=(const BusyGuard&) = delete;
};

// struct AttinyData — в core/types.h (подключается через setup.h)

uint8_t crc_8(const unsigned char *input_str, size_t num_bytes, uint8_t crc = 0);

class MasterI2C
{
    uint8_t init_crc = 0xFF;
    volatile bool i2c_busy;

protected:
    bool getUint(uint32_t &value, uint8_t &crc);
    bool getUint16(uint16_t &value, uint8_t &crc);
    bool getByte(uint8_t &value, uint8_t &crc);
    bool sendData(uint8_t *buf, size_t size);

    bool getByte(uint8_t *value, uint8_t &crc);
    bool getBytes(uint8_t *value, uint8_t count, uint8_t &crc);

public:
    MasterI2C();
    void begin();
    void end();
    bool sendCmd(uint8_t cmd);
    bool getMode(uint8_t &mode);
    bool getAttinyData(AttinyData &data);
    bool setWakeUpPeriod(uint16_t per);
    bool setCountersType(const uint8_t type0, const uint8_t type1);
    bool setTransmitMode();
    bool setSetupMode();
    bool setSleep();
    bool extendWakeUp();
    bool updateVoltage();
};

#endif