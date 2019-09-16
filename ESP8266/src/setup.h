#ifndef _WATERIUS_SETUP_h
#define _WATERIUS_SETUP_h

#include <Arduino.h>

#define FIRMWARE_VERSION "0.9.1"
  

/*
Версии прошивки для ESP

0.9.1 - 2019.09.16 - Замеряю просадку напряжения.
                     Совместим с attiny ver <=9, но для функции нужна ver 10.
0.9.0 - 2019.09.07 - WiFiManager ветка development
0.8.2 - 2019.05.19 - Ошибка +импульс при замкнутом положении НАМУР
                   - Автоматическое определение литров/импульс
0.8   - 2019.05.04 - Поддержка MQTT
0.7   - 2019.04.10 - Поддержка НАМУР
                   - Русскоязычный интерфейс настройки
                   - Отправка waterius.ru
                   - Проверка подключения счётчика в веб интерфейсе
0.6.1 - 2019.03.31 - Заголовки Waterius-Token, Waterius-Email
0.6   - 2019.03.23 - Поддержка HTTPS
0.5.4 - 2019.02.25 - обновил framework espressif8266 2.0.1 (arduino 2.5.0), blynk, json 
0.5.3 - 2019.01.22 - WifiManager 0.14 + hotfixes
0.5.2 - 2018.09.22 - WifiManager 0.14
*/ 
 
/* 
    Уровень логирования
*/
#define LOGLEVEL 6
//#define DEBUG_ESP_HTTP_CLIENT
//#define DEBUG_ESP_PORT Serial

/*
    Включить отправку данных на HTTP сервер
*/
#define SEND_WATERIUS

#define WATERIUS_DEFAULT_DOMAIN "https://cloud.waterius.ru"

/*
    Включить отправку данных в приложение Blynk.cc
*/
#define SEND_BLYNK

/*
    Включить отправку данных в MQTT
*/
#define SEND_MQTT

#ifdef ONLY_CLOUD_WATERIUS 
#undef SEND_BLYNK
#pragma message("SEND_BLYNK off")
#undef SEND_MQTT
#pragma message("SEND_MQTT off")
#endif

#define MQTT_DEFAULT_TOPIC_PREFIX "waterius/"  // Проверка: mosquitto_sub -h test.mosquitto.org -t "waterius/#" -v
#define MQTT_DEFAULT_PORT 1883


#define ESP_CONNECT_TIMEOUT 15000UL // Время подключения к точке доступа, ms

#define SERVER_TIMEOUT 12000UL // Время ответа сервера, ms


#define LITRES_PER_IMPULS_DEFAULT 10  // 10 литров на импульс

#define I2C_SLAVE_ADDR 10  // i2c адрес Attiny85

#define VER_5 5
#define CURRENT_VERSION VER_5

#define EMAIL_LEN 32

#define WATERIUS_KEY_LEN  34
#define WATERIUS_HOST_LEN 64

#define BLYNK_KEY_LEN 34
#define BLYNK_HOST_LEN 32

#define BLYNK_EMAIL_TITLE_LEN 64
#define BLYNK_EMAIL_TEMPLATE_LEN 200

#define MQTT_HOST_LEN 64
#define MQTT_LOGIN_LEN 32
#define MQTT_PASSWORD_LEN 32
#define MQTT_TOPIC_LEN 64


struct CalculatedData {
    float channel0;
    float channel1;
    uint32_t delta0;
    uint32_t delta1;

    uint32_t voltage_diff;
    bool  low_voltage;
};

/*
Настройки хранящиеся EEPROM
*/
struct Settings
{
    uint8_t  version;      //Версия конфигурации

    uint8_t  reserved;


    //SEND_WATERIUS
    
    //http/https сервер для отправки данных в виде JSON
    //вид: http://host[:port][/path]
    //     https://host[:port][/path]
    char     waterius_host[WATERIUS_HOST_LEN];
    char     waterius_key[WATERIUS_KEY_LEN];
    char     waterius_email[EMAIL_LEN];

    //SEND_BLYNK 

    //уникальный ключ устройства blynk
    char     blynk_key[BLYNK_KEY_LEN];
    //сервер blynk.com или свой blynk сервер
    char     blynk_host[BLYNK_HOST_LEN];

    //Если email не пустой, то отсылается e-mail
    //Чтобы работало нужен виджет эл. почта в приложении
    char     blynk_email[EMAIL_LEN];
    //Заголовок письма. {V0}-{V4} заменяются на данные 
    char     blynk_email_title[BLYNK_EMAIL_TITLE_LEN];
    //Шаблон эл. письма. {V0}-{V4} заменяются на данные 
    char     blynk_email_template[BLYNK_EMAIL_TEMPLATE_LEN];

    char     mqtt_host[MQTT_HOST_LEN];
    uint16_t mqtt_port;
    char     mqtt_login[MQTT_LOGIN_LEN];
    char     mqtt_password[MQTT_PASSWORD_LEN];
    char     mqtt_topic[MQTT_TOPIC_LEN];

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
    uint32_t impulses0_start;
    uint32_t impulses1_start;

    /*
    Не понятно, как получить от Blynk прирост показаний, 
    поэтому сохраним их в памяти каждое включение
    */
    uint32_t impulses0_previous;
    uint32_t impulses1_previous;

    /*
    Зарезервируем кучу места, чтобы не писать конвертер конфигураций.
    Будет актуально для On-the-Air обновлений
    */
    uint8_t  reserved2[256];

    /*
    Контрольная сумма, чтобы гарантировать корректность чтения настроек
    */
    uint16_t crc;
}; //976 байт

#endif