#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>

#define FIRMWARE_VERSION "0.5.3"

/*
Версии прошивки для ESP

0.5.3 - 2019.01.22 - WifiManager 0.14 + hotfixes
0.5.2 - 2018.09.22 - WifiManager 0.14
*/ 

/*
    Включить отправку данных на свой TCP сервер. см. sender_tcp.h
*/
//#define SEND_TCP   

/*
    Включить отправку данных в приложение Blynk.cc
*/
#define SEND_BLYNK

/*
    Включить отправку данных на HTTP сервер
*/
#define SEND_JSON

/*
    Уровень логирования
*/
#define LOGLEVEL 6

/*
    Время ответа сервера
*/
#define SERVER_TIMEOUT 5000UL // ms

/*
    Время подключения к точке доступа
*/
#define ESP_CONNECT_TIMEOUT 13000UL


#define LITRES_PER_IMPULS_DEFAULT 10  // При первом включении заполним 10 литров на импульс

#define I2C_SLAVE_ADDR 10  // i2c адрес Attiny85

#define VER_4 4
#define CURRENT_VERSION VER_4


#define KEY_LEN 34
#define HOSTNAME_LEN 32

#define EMAIL_LEN 32
#define EMAIL_TITLE_LEN 64
#define EMAIL_TEMPLATE_LEN 200

#define HOSTNAME_JSON_LEN 64

/*
Настройки хранящиеся EEPROM
*/
struct Settings
{
    uint8_t  version;      //Версия конфигурации

    uint8_t  reserved;
    
    /*
    SEND_BLYNK: уникальный ключ устройства blynk
    SEND_TCP: не используется
    */
    char     key[KEY_LEN];

    /*
    SEND_BLYNK: сервер blynk.com или свой blynk сервер
    SEND_TCP: ip адрес или имя хоста куда слать данные
    */
    char     hostname[HOSTNAME_LEN];

    /*
    SEND_BLYNK: Если email не пустой, то отсылается e-mail
    SEND_TCP: не используется    
    */
    char     email[EMAIL_LEN];
    
    /*
    SEND_BLYNK: Заголовок письма. {V0}-{V4} заменяются на данные 
    SEND_TCP: не используется    
    */
    char     email_title[EMAIL_TITLE_LEN];

    /*
    SEND_BLYNK: Шаблон эл. письма. {V0}-{V4} заменяются на данные 
    SEND_TCP: не используется    
    */
    char     email_template[EMAIL_TEMPLATE_LEN];
    
    /*
    http сервер для отправки данных в виде JSON
    в виде: http://host:port/path
    */
    char     hostname_json[HOSTNAME_JSON_LEN];

    /*
    Показания счетчиках в кубометрах, 
    введенные пользователем при настройке
    */
    float    channel0_start;
    float    channel1_start;

    /*
    Кол-во литров на 1 импульс
    */
    uint16_t liters_per_impuls;

    /*
    Кол-во импульсов Attiny85 соответствующие показаниям счетчиков, 
    введенных пользователем при настройке
    */
    uint32_t impules0_start;
    uint32_t impules1_start;

    /*
    Не понятно, как получить от Blynk прирост показаний, 
    поэтому сохраним их в памяти каждое включение
    */
    float    channel0_previous;
    float    channel1_previous;

    /*
    Зарезервируем кучу места, чтобы не писать конвертер конфигураций.
    Будет актуально для On-the-Air обновлений
    */
    uint8_t  reserved2[256];

    /*
    Контрольная сумма, чтобы гарантировать корректность чтения настроек
    */
    uint16_t crc;
};

#endif