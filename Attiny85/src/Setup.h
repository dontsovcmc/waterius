#ifndef _WATERIUS_SETUP_h
#define _WATERIUS_SETUP_h

#include <Arduino.h>

/*
    Включение логирования
    3 pin TX -> RX (TTL-USB 3.3 или 5в), 9600 8N1
    При логировании не работает счетчик2 на 3-м пине (Вход 2).

    #define LOG_ON
*/

/*
    Включение отладки сигнала со счетчиков на пин питания ESP
    
    #define COUNTER_DEBUG
*/

#ifndef LOG_ON
#define LOG_BEGIN(x)
#define LOG(x)
#else
#undef LOG_BEGIN
#undef LOG

// TinyDebugSerial на PB3 только в attiny85, 1MHz
#include "TinyDebugSerial.h"
#define LOG_BEGIN(x) DDRB |= _BV(3); mySerial.begin(x)
#define LOG(x)                \
    mySerial.print(millis()); \
    mySerial.print(F(" : ")); \
    mySerial.println(x);
#endif

/*
    Классический Ватериус
 */
#define MODEL_CLASSIC 0

/*
    Ватериус-мини
*/
#define MODEL_MINI    1

/*
    1 минута примерно равна 240 пробуждениям
*/
#define ONE_MINUTE 240L

/*
    Период отправки данных на сервер, мин.
    Если перезагрузилось устройство.
    ESP пришлёт корректировку.
    1440 - 10% - 1300 мин. 
*/
#define WAKEUP_PERIOD_DEFAULT 1300L * ONE_MINUTE


/*
    Аварийное отключение, если ESP зависнет и не пришлет команду "сон".
*/
#define WAIT_ESP_MSEC 120000UL

/*
    Сколько милисекунд пользователь может
    настраивать ESP. Если не закончил, питание ESP выключится.
*/
#define SETUP_TIME_MSEC 600000UL

/*
    время долгого нажатия кнопки, милисекунд
*/
#define LONG_PRESS_MSEC 2000

/*
    пауза мс перед переходом ESP в сон (чтобы успел отключиться)
*/
#define DELAY_SENT_SLEEP 20000
    

/*
    стандартная константа калибровки вольтметра в attiny
 */
#define DEFAULT_AVR_VCC_REFERENCE_MV 1100  


struct Data
{
    uint32_t value0;
    uint32_t value1;
};

struct CounterTypes
{
    uint8_t type0; // тип входа
    uint8_t type1;
};

struct ADCLevel
{
    uint16_t adc0;
    uint16_t adc1;
};

struct Config
{
    uint8_t setup_started_counter;      // Включение режима настройки
    uint8_t resets;                     // Количество перезагрузок
    CounterTypes types;                 // Типы входов счетчиков
    uint16_t v_reference;               // Калибровочная константа
};

struct Header
{

    /*
    Версия прошивки
    */
    uint8_t version;

    /* 
        Модификация Ватериуса 
    */
    uint8_t model:   4;

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
    uint8_t service: 4;

    /*
    Напряжение
    */
    uint16_t voltage;

    /*
    Конфигурация
    */
    Config config;       // 6 байт

    /*
    Текущие данные
    */
    Data data;           // 8 байт
    ADCLevel adc;        // 4 байта

    // HEADER_DATA_SIZE

    uint8_t crc;
    uint8_t reserved3;
}; // 24 байт

#define HEADER_DATA_SIZE 22

#define TX_BUFFER_SIZE HEADER_DATA_SIZE + 2

#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
#define ct_assert(e) enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(!!(e)) }

ct_assert(sizeof(Header)==24);

#endif