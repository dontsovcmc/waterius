#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>


/*
в Platformio 
#define ARDUINO_AVR_ATTINYX5   // attiny85 
#define ARDUINO_AVR_ATTINYX4   // attiny84 
*/

#ifdef ARDUINO_AVR_ATTINYX5
#define WATERIUS_2C 0    // attiny85 - 2 счетчика импульсов
#else
#ifdef ARDUINO_AVR_ATTINYX4
#define WATERIUS_4C2W 1  // attiny84 - 4 счетчика импульсов
#else
#error "Please, #define ARDUINO_AVR_ATTINYX5 for Attiny85 or ARDUINO_AVR_ATTINYX4 for Attiny84"
#endif
#endif 


/* 
	Включение логирования
	3 pin TX -> RX (TTL-USB 3.3 или 5в), 9600 8N1
	При логировании не работает счетчик2 на 3-м пине (Вход 2).

    #define LOG_ON
*/

#define LOG_BEGIN(x)
#define LOG(x)

#ifdef LOG_ON
#include "TinyDebugSerial.h"
    class TinyDebugSerial;
    extern TinyDebugSerial mySerial;
    #undef LOG_BEGIN
    #define LOG_BEGIN(x)  mySerial.begin(x)
    #undef LOG
    #define LOG(x) mySerial.print(millis()); mySerial.print(F(" : ")); mySerial.println(x);
#endif

//#define TEST_WATERIUS   // Тестирование счетчика при помощи Arduino

/*
	Период отправки данных на сервер, мин
*/
#ifdef TEST_WATERIUS
#define WAKE_EVERY_MIN   10
#else
#define WAKE_EVERY_MIN   24 * 60U  // 1U 
#endif

/*
	Аварийное отключение, если ESP зависнет и не пришлет команду "сон".
*/
#define WAIT_ESP_MSEC    120000UL      

/*
	Сколько милисекунд пользователь может 
	настраивать ESP. Если не закончил, питание ESP выключится.
*/
#define SETUP_TIME_MSEC  600000UL 

/*
    время долгого нажатия кнопки, милисекунд 
*/
#define LONG_PRESS_MSEC  3000   
       

struct Data {
    uint32_t value0;
    uint32_t value1;
};

struct CounterState { // не добавляем в Data, т.к. та в буфере кольцевом
    uint8_t  state0;  // состояние входа
    uint8_t  state1;  
};

struct Header {

    /*
    Версия прошивки
    */
    uint8_t       version;
    
    /*
    Причина перезагрузки (регистр MCUSR datasheet 8.5.1):
         0001 - PORF: Power-on Reset Flag. Напряжение питания было низкое или 0.
         0010 - EXTRF: External Reset Flag. Пин ресет был в низком уровне.
         0100 - BORF: Brown-out Reset Flag. Напряжение питание было ниже требуемого. 
         1000 - WDRF: Watchdog Reset Flag. Завершение работы таймера.

    8  - 1000 - WDRF
    9  - 1001 - WDRF + PORF
    10 - 1010 - WDRF + EXTRF
    */
    uint8_t       service; 

    /*
    Напряжение питания в мВ.
    */
    uint32_t      voltage;
    
    /*
    Количество перезагрузок.
    */
    uint8_t       resets;

    /*
    Модификация
    */
    uint8_t       model;

    CounterState  states;
    Data          data;
    uint16_t      adc0;    // уровень входа
    uint16_t      adc1; 
    // HEADER_DATA_SIZE

    uint8_t       crc;
    uint8_t       reserved2;
};  //22 байт

#define HEADER_DATA_SIZE 22
#define TX_BUFFER_SIZE HEADER_DATA_SIZE + 2

#endif