#ifndef _MASTERI2C_h
#define _MASTERI2C_h

#include <Arduino.h>

/*
Номера пинов линии i2c.
*/
#define SDA_PIN 0
#define SCL_PIN 2

/*
Данные принимаемые от Attiny
*/

struct AttinyData
{
    // Header
    uint8_t version;    // Версия ПО Attiny
    uint8_t model;      //:4 бита Модель Ватериуса  # WateriusModel
    uint8_t service;    //:4 бита Причина загрузки Attiny
    uint16_t voltage; // Напряжение питания, мВ
    
    // Config
    uint8_t setup_started_counter; // Кол-во стартов режима настройки
    uint8_t resets;     // Кол-во перезагрузок
    uint8_t counter_type0; // Тип входа, вход 0
    uint8_t counter_type1; //           вход 1
    uint16_t v_reference;               // Калибровочная константа

    // 
    uint32_t impulses0;    // Импульсов, канал 0
    uint32_t impulses1;    //           канал 1
    //
    uint16_t adc0;         // Уровень,   канал 0
    uint16_t adc1;         //           канал 1

    // HEADER_DATA_SIZE

    uint8_t crc = 0; // Всегда в конце структуры данных
    uint8_t reserved2 = 0;
};  // 24 bytes

uint8_t crc_8(const unsigned char *input_str, size_t num_bytes, uint8_t crc = 0);

#define INIT_ATTINY_CRC 0xFF

// Класс для синхронизации запросов к i2c, которые могут быть из разных потоков в AsyncWebServer
// В ESP отсутствуют мьютексы, поэтому будем считать, что это проще сдела
class BusyGuard {
    volatile bool& busy;
public:
    BusyGuard(volatile bool& flag) : busy(flag) {
        while (busy) yield(); // Ждём, пока флаг не сброшен
        busy = true;
    }
    ~BusyGuard() {
        busy = false;
    }
    BusyGuard(const BusyGuard&) = delete;
    BusyGuard& operator=(const BusyGuard&) = delete;
};

class MasterI2C
{
    volatile bool i2c_busy;
protected:
    bool sendData(uint8_t *buf, size_t size);

    bool getByte(uint8_t &value, uint8_t &crc);
    bool getUint16(uint16_t &value, uint8_t &crc);
    bool getUint32(uint32_t &value, uint8_t &crc);
    bool sendCmd(uint8_t cmd);

public:
    MasterI2C();
    void begin();
    void end();
    bool getMode(uint8_t &mode);
    bool getSlaveData(AttinyData &data);
    bool setWakeUpPeriod(uint16_t per);
    bool setCountersType(const uint8_t type0, const uint8_t type1);
    bool setReferenceVoltage(uint16_t voltage);
    bool setTransmitMode();
    bool setSleep();
};

#endif