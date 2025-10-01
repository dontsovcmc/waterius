#ifndef _WATERIUS_SETUP_h
#define _WATERIUS_SETUP_h

#include <Arduino.h>

#ifndef FIRMWARE_VERSION
#error "Please define environment variable FIRMWARE_VERSION=x.x.x"
#endif 

/*
Версии прошивки для ESP

1.1.1  - 2024.02.05 - dontsovcmc

*/

/*
    Уровень логирования
*/


#define BRAND_NAME "waterius"

#define WATERIUS_DEFAULT_DOMAIN "https://cloud.waterius.ru"

#define ESP_CONNECT_TIMEOUT 10000UL // Время подключения к точке доступа, ms

#define SERVER_TIMEOUT 12000UL // Время ответа сервера, ms

#define I2C_SLAVE_ADDR 10 // i2c адрес Attiny85

#define VER_8 8
#define VER_9 9
#define VER_10 10
#define VER_11 11
#define CURRENT_VERSION VER_11

#define EMAIL_LEN 40

#define WATERIUS_KEY_LEN 34
#define HOST_LEN 64

#define BLYNK_RESERVED 98


#define MQTT_LOGIN_LEN 32
#define MQTT_PASSWORD_LEN 66 //ansible образ home assistant генерирует пароль длиной 64
#define MQTT_TOPIC_LEN 64

#define MQTT_DEFAULT_TOPIC_PREFIX BRAND_NAME // Проверка: mosquitto_sub -h test.mosquitto.org -t "waterius/#" -v
#define MQTT_DEFAULT_PORT 1883

#ifndef DISCOVERY_TOPIC
#define DISCOVERY_TOPIC "homeassistant"
#endif

#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY true // если true то публикуется автодискавери топик для Home Assistant
#endif

#define MQTT_FORCE_UPDATE true // Сенсор в HA будет обновляться даже если значение не обновилось

#define CHANNEL_NUM 2

#define HARDWARE_VERSION "1.0.0"
#define MANUFACTURER "Waterius"

#define JSON_DYNAMIC_MSG_BUFFER 2048
#define JSON_SMALL_STATIC_MSG_BUFFER 256

#define ROUTER_MAC_LENGTH 8
#define MAC_LENGTH 18
#define IP_LENGTH 16

#define SERIAL_LEN 16

#ifndef DEFAULT_WAKEUP_PERIOD_MIN
#define DEFAULT_WAKEUP_PERIOD_MIN 1440
#endif

#define AUTO_IMPULSE_FACTOR 3
#define AS_COLD_CHANNEL 7

#define DEF_FALLBACK_DNS "8.8.8.8"

#define WIFI_CONNECT_ATTEMPTS 2

#define WIFI_SSID_LEN 32
#define WIFI_PWD_LEN 64

#define DEFAULT_GATEWAY "192.168.0.1"
#define DEFAULT_MASK "255.255.255.0"
#define DEFAULT_NTP_SERVER "ru.pool.ntp.org"

#ifndef LED_PIN
#define LED_PIN 1
#endif

// attiny85
#define SETUP_MODE 1
#define TRANSMIT_MODE 2
#define MANUAL_TRANSMIT_MODE 3

// model
#define WATERIUS_CLASSIC 0
#define WATERIUS_4C2W 1

enum CounterType : uint8_t
{
    NAMUR = 0,
    DISCRETE = 1,
    ELECTRONIC = 2,
    HALL = 3, 
    NONE = 0xFF 
};

enum CounterName
{
    WATER_COLD = 0,
    WATER_HOT = 1,
    ELECTRO = 2,
    GAS = 3,
    HEAT = 4,
    PORTABLE_WATER = 5,
    OTHER = 6
};

// согласно
enum DataType
{
    COLD_WATER = 0,
    HOT_WATER = 1,
    ELECTRICITY = 2,
    GAS_DATA = 3,
    HEATING = 4,
    ELECTRICITY_DAY = 5,
    ELECTRICITY_NIGHT = 6,
    ELECTRICITY_PEAK = 7,
    ELECTRICITY_HALF_PEAK = 8,
    POTABLE_WATER = 9,
    OTHER_TYPE = 10
};

struct CalculatedData
{
    // Показания в кубометрах
    float channel0 = 0.0;
    // Показания в кубометрах
    float channel1 = 0.0;

    uint32_t delta0 = 0;
    uint32_t delta1 = 0;
};

/*
Настройки хранящиеся EEPROM
*/
struct Settings
{
    uint8_t version = CURRENT_VERSION; // Версия конфигурации

    uint8_t reserved0 = 0;

    // SEND_WATERIUS

    // http/https сервер для отправки данных в виде JSON
    // вид: http://host[:port][/path]
    //      https://host[:port][/path]
    char waterius_host[HOST_LEN] = {0};
    char waterius_key[WATERIUS_KEY_LEN] = {0};
    char waterius_email[EMAIL_LEN] = {0};

    // 
    char reserved_blynk[BLYNK_RESERVED] = {0};

    char http_url[HOST_LEN] = {0};

    char mqtt_host[HOST_LEN] = {0};
    uint16_t mqtt_port = MQTT_DEFAULT_PORT;
    char mqtt_login[MQTT_LOGIN_LEN] = {0};
    char mqtt_password[MQTT_PASSWORD_LEN] = {0};
    char mqtt_topic[MQTT_TOPIC_LEN] = {0};

    /*
    Показания счетчиках в кубометрах,
    введенные пользователем при настройке
    */
    float channel0_start = 0.0;
    float channel1_start = 0.0;

    /*
    Настройка типа входа счетчиков
    */
    CounterType counter_type0 = NAMUR;
    CounterType counter_type1 = NAMUR;

    /*
    Серийные номера счётчиков воды
    */
    char serial0[SERIAL_LEN] = {0};
    char serial1[SERIAL_LEN] = {0};

    /*
    Кол-во импульсов Attiny85 соответствующие показаниям счетчиков,
    введенных пользователем при настройке
    */
    uint32_t impulses0_start = 0;
    uint32_t impulses1_start = 0;

    /*
    Прирост показаний. Каждое включение
    */
    uint32_t impulses0_previous = 0;
    uint32_t impulses1_previous = 0;

    /*
    Время последнего пробуждения
    */
    uint32_t wake_time = 0;

    /*
    За сколько времени настроили ватериус
    */
    uint32_t setup_time = 0;

    /*
    Статический адрес
    */
    uint32_t ip = 0;
    uint32_t gateway = 0;
    uint32_t mask = 0;

    /*
    Период пробуждение для отправки данных, мин
    */
    uint16_t wakeup_per_min = DEFAULT_WAKEUP_PERIOD_MIN;

    /*
    Установленный период отправки с учетом погрешности
    */
    uint16_t set_wakeup = DEFAULT_WAKEUP_PERIOD_MIN;

    /*
    Время последней отправки по расписанию
    */
    time_t last_send = 0; // Size of time_t: 8

    /*
    Режим пробуждения
    */
    uint8_t mode = SETUP_MODE; // SETUP_MODE

    /*
    Успешная настройка
    */
    uint8_t setup_finished_counter = 0;

    /* Публиковать данные для автоматического добавления в Homeassistant */
    uint8_t mqtt_auto_discovery = (uint8_t)MQTT_AUTO_DISCOVERY;
    uint8_t ntp_error_counter = 0;

    /* Топик MQTT*/
    char mqtt_discovery_topic[MQTT_TOPIC_LEN] = DISCOVERY_TOPIC;

    /* пользовательский NTP сервер */
    char ntp_server[HOST_LEN] = {0};

    /* имя сети Wifi */
    char wifi_ssid[WIFI_SSID_LEN] = {0};
    /* пароль к Wifi сети */
    char wifi_password[WIFI_PWD_LEN] = {0};
    /* mac сети Wifi */
    uint8_t wifi_bssid[6] = {0};
    /* Wifi канал */
    uint8_t wifi_channel = 1;
    /* Режим работы интерфейса */
    uint8_t wifi_phy_mode = 0;

    /*
    Тип счётчика (вода, тепло, газ, электричество)
    */
    uint8_t counter0_name = CounterName::WATER_HOT;
    uint8_t counter1_name = CounterName::WATER_COLD;

    /*
    Кол-во литров на 1 импульс
    */
    uint16_t factor0 = AS_COLD_CHANNEL;
    uint16_t factor1 = AUTO_IMPULSE_FACTOR;

    /* Включение передачи на офиц. сайт */
    uint8_t waterius_on = (uint8_t) true;
    /* Включение передачи по http на другой хост */
    uint8_t http_on = (uint8_t) false;
    /* Включение передачи по mqtt */
    uint8_t mqtt_on = (uint8_t) false;
    
    uint8_t reserved4 = 0;
    /* Включение DHCP или статических настроек */
    uint8_t dhcp_off = (uint8_t) false;

    uint8_t reserved8 = 0;
    /*
    Зарезервируем кучу места, чтобы не писать конвертер конфигураций.
    Будет актуально для On-the-Air обновлений
    */
    uint8_t reserved9[84] = {0};
}; // 960 байт

#endif