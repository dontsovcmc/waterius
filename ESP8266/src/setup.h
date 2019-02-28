#ifndef _WATERIUS_SETUP_h
#define _WATERIUS_SETUP_h

#include <Arduino.h>

#define FIRMWARE_VERSION "0.5.4"

#define DEBUG_ESP_HTTP_CLIENT
#define DEBUG_ESP_PORT Serial

/*
Версии прошивки для ESP

0.5.4 - 2019.02.25 - обновил framework espressif8266 2.0.1 (arduino 2.5.0), blynk, json 
0.5.3 - 2019.01.22 - WifiManager 0.14 + hotfixes
0.5.2 - 2018.09.22 - WifiManager 0.14
*/ 
  

/*
    Включить отправку данных в приложение Blynk.cc
*/
//#define SEND_BLYNK

/*
    Включить отправку данных на HTTP сервер
*/
//#define SEND_JSON

#define SEND_HTTPS

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

#define VER_5 5
#define CURRENT_VERSION VER_5


#define KEY_LEN 34
#define HOSTNAME_BLYNK_LEN 32

#define EMAIL_LEN 32
#define EMAIL_TITLE_LEN 64
#define EMAIL_TEMPLATE_LEN 200

#define HOSTNAME_JSON_LEN 64

#define CERT_LEN 2000
/*
Настройки хранящиеся EEPROM
*/
struct Settings
{
    uint8_t  version;      //Версия конфигурации

    uint8_t  reserved;

    //SEND_BLYNK 

    //уникальный ключ устройства blynk
    char     key[KEY_LEN];
    //сервер blynk.com или свой blynk сервер
    char     hostname_blynk[HOSTNAME_BLYNK_LEN];

    //Если email не пустой, то отсылается e-mail
    char     email[EMAIL_LEN];
    //Заголовок письма. {V0}-{V4} заменяются на данные 
    char     email_title[EMAIL_TITLE_LEN];
    //Шаблон эл. письма. {V0}-{V4} заменяются на данные 
    char     email_template[EMAIL_TEMPLATE_LEN];

    //SEND_JSON
    
    //http сервер для отправки данных в виде JSON
    //вид: http://host:port/path
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
    Сертификат
    */
    char     ca[CERT_LEN];

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