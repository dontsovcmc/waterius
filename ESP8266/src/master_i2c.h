#ifndef _MASTERI2C_h
#define _MASTERI2C_h

#include <Arduino.h>

/*
Номера пинов линии i2c.
В зависимости от модификации ESP8266 может быть разным
*/
#define SDA_PIN 0
#define SCL_PIN 2

//attiny85
#define SETUP_MODE 1
#define TRANSMIT_MODE 2


/*
Данные принимаемые от Attiny
*/
struct SlaveData {
    uint8_t  version;     //Версия ПО Attiny
    uint8_t  service;     //Причина загрузки Attiny
    uint32_t voltage;     //Напряжение питания в мВ
    uint8_t  state0;      //Состояние входа 0
    uint8_t  state1;      //Состояние входа 1
    uint32_t impulses0;   //Импульсов, канал 0
    uint32_t impulses1;   //Импульсов, канал 1
    uint8_t  diagnostic;  //1 - ок, 0 - нет связи с Attiny
    uint8_t  resets;   
    //Кратно 16bit https://github.com/esp8266/Arduino/issues/1825
}; 


class MasterI2C
{
protected:
    bool getUint(uint32_t &value);
    bool getByte(uint8_t &value);
    
public:
    void begin();
    void end();
    bool sendCmd( const char cmd );
    bool getMode(uint8_t &mode);
    bool getSlaveData(SlaveData &data);
};


#endif

