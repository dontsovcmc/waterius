#ifndef _MASTERI2C_h
#define _MASTERI2C_h

#include <Arduino.h>

/*
Номера пинов линии i2c.
*/
#define SDA_PIN 0
#define SCL_PIN 2

//attiny85
#define SETUP_MODE 1
#define TRANSMIT_MODE 2
#define MANUAL_TRANSMIT_MODE 3

//model
#define WATERIUS_CLASSIC 0
#define WATERIUS_4C2W    1

enum Status_t {
    WATERIUS_NO_LINK = 0,  //нет связи по i2c
    WATERIUS_OK = 1,
    WATERIUS_BAD_CRC = 2
};

/*
Данные принимаемые от Attiny
*/
struct SlaveData {  
    // Header
    uint8_t  version;     //Версия ПО Attiny
    uint8_t  service;     //Причина загрузки Attiny
    uint32_t voltage;     //Напряжение питания в мВ (после включения wi-fi под нагрузкой )

    uint8_t  resets;   
    uint8_t  model;       //WATERIUS_CLASSIC или  WATERIUS_4C2W 
    uint8_t  state0;      //Состояние, вход 0
    uint8_t  state1;      //           вход 1
    uint32_t impulses0;   //Импульсов, канал 0
    uint32_t impulses1;   //           канал 1
    uint16_t adc0;        //Уровень,   канал 0
    uint16_t adc1;        //           канал 1
    
    // HEADER_DATA_SIZE

    uint8_t  crc;         //Всегда в конце структуры данных
    uint8_t  reserved2;   
    
    enum Status_t diagnostic; 
    uint8_t  reserved3;  
    //Кратно 16bit https://github.com/esp8266/Arduino/issues/1825
}; 


uint8_t crc_8( const unsigned char *input_str, size_t num_bytes, uint8_t crc = 0);

class MasterI2C
{
protected:
    bool getUint(uint32_t &value, uint8_t &crc);
    bool getUint16(uint16_t &value, uint8_t &crc);
    bool getByte(uint8_t &value, uint8_t &crc);
    bool sendData(uint8_t* buf, size_t size);
public:
    void begin();
    void end();
    bool sendCmd( uint8_t cmd );
    bool getMode(uint8_t &mode);
    bool getSlaveData(SlaveData &data);
    bool setWakeUpPeriod(uint16_t per);
};


#endif

