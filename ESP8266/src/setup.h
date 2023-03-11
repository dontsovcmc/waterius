#pragma once
#ifndef _WATERIUS_SETUP_h
#define _WATERIUS_SETUP_h

#include <Arduino.h>

#define FIRMWARE_VERSION "0.11.1"

// Конвертируем значение переменных компиляции в строк
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "=" VALUE(var)

/*
Версии прошивки для ESP

0.11.1 - 2023.02.28 - neitri, dontsovcmc
                      1. Указанный пользователем NTP сервер используется.

0.11.0 - 2023.01.23 - dontsovcmc Anat0liyBM vzagorovskiy
                      1. PubSubClient 2.7.0 -> 2.8.0
                      2. Отправка описания параметров в HomeAssistant
                      3. В поля данных
                      - mac переименован в router_mac, формат шестнадцатиричный разделенный двоеточием
                      - mac - MAC адрес ESP, формат шестнадцатиричный разделенный двоеточием
                      - esp_id - id ESP, в десятичном формате
                      - ip - IP адрес ESP
                      4. ArduinoJson 6.15.1->6.18.3
                      5. Формат имени точки доступа waterius-ИДЕНТИФИКАТОР_ЕСП-НОМЕР_ВЕРСИИ_ПРОШИВКИ
                      6. Имя хоста изменено на waterius-ИДЕНТИФИКАТОР_ЕСП идентиификтр в десятисном виде
                      7. Формирование одного JSON для публикации по MQTT и HTTP
                      8. Возможность публиковать всю информацию в один топик MQTT в формате JSON
                      9. Установка часов выполняется вне зависимости будет ли запрос по https. Время используется для MQTT.
                      10. В класс Voltage добавлен метод измерения % батареи, немного исправлен признак севшей батареи.
                      11. Оптимизировано использование памяти при работе по https
                      12. Добавлена возможность использования самоподписанных сертификатов
                      13. После настройки устройства автодискавери топики будут удалены, т.к. пользователь мог именить форматы.
                      14. Убраны глобальные переменные для https и mqtt чтобы сэкономить память
                      15. Добавлена публикация вспомогательных показаний через json_attributes при автодискавери в HA, что позволило сильно сократить кол-во запросов
                      16. Добавлена опция для сенсовров в HA, force_update сенсор будет обновляться при получении сообщения даже если значение не изменилось
                      17. Доработано измерение напряжения, теперь отправляются усредненные показания напряжения.
                      18. Напряжение измеряется в фоне раз в 300мс
                      19. Добавлены признаки интеграции с HA, MQTT, blynk
                      20. Добавлена подписка на изменения параметров в HA
                      21. Добавлена кастомная реализация синхронизации времени по NTP
                      22. Добавлены функции по корректному подключению/отключением от WIFI при режиме глубокого сна
                      23. Сохраняется послений успешный  BSSID и канал точки доступа для быстрого подключения к WIFI
                      24. Рефакторинг функции отправки на сайт
                      25. Добавлена возможность пользователю указать свой NTP сервер, если не удалось с этого сервера получить время то будет браться время по серврам из пула

0.10.7 - 2022.04.20 - dontsovcmc
                      1. issues/227: не работали ssid, pwd указанные при компиляции

0.10.6 - 2022.02.18 - neitri, dontsovcmc
                      1. espressif8266@3.2.0
                      2. attiny версия 24
                      3. период отправки 24ч (корректируется по NTP. точность +-1 мин)
                      4. передача данных после настройки ESP
                      5. добавлены параметры:
                      - режим пробуждения. теперь видно, что вручную кнопка нажата
                      - число включения режима настройки
                      - число успешных подключений к роутеру после настройки
                      - номер канала Wi-Fi
                      - MAC адрес производителя роутра (первые 3 байта)
                      6. чтение напряжения ESP
                      7. В списке подключенных устройств роутера теперь Waterius-X

0.10.5 - 2021.07.18 - attiny версия 22

0.10.4 - 2021.06.20 - Добавил серийные номера

0.10.3 - 2021.04.02 - Исправления в прошивке attiny

0.10.2 - 2021.02.30 - Обновлены сертификаты Lets Encrypt

0.10.1 - 2021.02.08 - Добавлена настройка веса импульса для горячего
                      и холодного счетчика. Добавлена настройка периода
                      отправки данных.

0.10.0 - 2020.06.16 - Поддержка версии 4C2W (на attiny85)

0.9.13 - 2020.05.16 - проверка crc в данных от attiny
                      добавлен статический ip, gateway, mask
                      отображение MAC адреса в настройках
                      пароль не передается в веб интерфейс
                      обновление фреймворка до 2.5.1 требует переписи синхронизации времени
0.9.12 - 2020.04.27 - обновил platform-espressif8266.git#v2.4.0
0.9.11 - 2020.03.04 - mqtt retain=true
0.9.10 - 2019.12.29 - Точность показаний в mqtt и интерфейсе: 0.001.
                      Исправлена разница в литрах после настройки. Спасибо @sintech!
0.9.9 - 2019.12.21 - Исправлен html код списка SSID для поддержки спец. символов в названии SSID
                     Индикатор низкого заряда 50мВ
0.9.8 - 2019.12.11 - точность ввода показаний 0.001
0.9.7 - 2019.12.10 - обновил platform-espressif8266.git#v2.3.1 (Arduino core 2.6.2)
                   - (в 0.9.6 были проблемы с DHCP, перезагружался при настройке. кажется лечится erase_memory)
0.9.6 - 2019.11.26 - Добавил RSSI, косметика.
                   - Не надо повторно вводить имя сети и пароль
                   - Динамическая загрузка Wi-Fi. Теперь памяти хватит.
0.9.5 - 2019.11.15 - оптимизировал память WifiManager, иначе веб страница могла не догрузиться
0.9.4 - 2019.11.05 - убрал константы SEND_MQTT, SEND_BLYNK, ONLY_CLOUD_WATERIUS
                   - добавил f - вес импульса в даннные mqtt, http
                   - улучшен интерфейс настройки
0.9.3 - 2019.10.27 - Исправил random
0.9.2 - 2019.10.16 - Исправил поля в json (boot, version)
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

// уровни логирования WifiManager
#ifndef DWM_DEBUG_LEVEL
#define DWM_DEBUG_LEVEL 0
#endif

#define BRAND_NAME "waterius"

#define WATERIUS_DEFAULT_DOMAIN "https://cloud.waterius.ru"

#ifndef WATERIUS_HOST
#define WATERIUS_HOST WATERIUS_DEFAULT_DOMAIN
#else
#pragma message(VAR_NAME_VALUE(WATERIUS_HOST))
#endif

#ifndef WATERIUS_KEY
#define WATERIUS_KEY ""
#else
#pragma message(VAR_NAME_VALUE(WATERIUS_KEY))
#endif

#define ESP_CONNECT_TIMEOUT 10000UL // Время подключения к точке доступа, ms

#define SERVER_TIMEOUT 12000UL // Время ответа сервера, ms

#define I2C_SLAVE_ADDR 10 // i2c адрес Attiny85

#define VER_8 8
#define CURRENT_VERSION VER_8

#define EMAIL_LEN 40

#define WATERIUS_KEY_LEN 34
#define HOST_LEN 64

#define BLYNK_KEY_LEN 34

#define BLYNK_EMAIL_TITLE_LEN 64
#define BLYNK_EMAIL_TEMPLATE_LEN 200

#define MQTT_LOGIN_LEN 32
#define MQTT_PASSWORD_LEN 32
#define MQTT_TOPIC_LEN 64

#define MQTT_DEFAULT_TOPIC_PREFIX BRAND_NAME // Проверка: mosquitto_sub -h test.mosquitto.org -t "waterius/#" -v
#define MQTT_DEFAULT_PORT 1883U

#ifndef DISCOVERY_TOPIC
#define DISCOVERY_TOPIC "homeassistant"
#endif

#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY true // если true то публикуется автодискавери топик для Home Assistant
#endif

#ifndef ALWAYS_MQTT_AUTO_DISCOVERY
#define ALWAYS_MQTT_AUTO_DISCOVERY false // если true то всегда публикуется автодискавери топик для Home Assistant при отправке данных
#endif

#define MQTT_FORCE_UPDATE true // Сенсор в HA будет обновляться даже если значение не обновилось

#define CHANNEL_NUM 2

#define HARDWARE_VERSION "1.0.0"
#define MANUFACTURER "Waterius"

#define JSON_DYNAMIC_MSG_BUFFER 1024
#define JSON_STATIC_MSG_BUFFER 512
#define JSON_SMALL_STATIC_MSG_BUFFER 256

#define ROUTER_MAC_LENGTH 8
#define MAC_LENGTH 18
#define IP_LENGTH 16

#define SERIAL_LEN 16

#ifndef DEFAULT_WAKEUP_PERIOD_MIN
#define DEFAULT_WAKEUP_PERIOD_MIN 1440
#endif

#define AUTO_IMPULSE_FACTOR 2
#define AS_COLD_CHANNEL 7

#define DEF_FALLBACK_DNS "8.8.8.8"

#define WIFI_CONNECT_ATTEMPTS 3

#define WIFI_SSID_LEN 32 + 1
#define WIFI_PWD_LEN 64 + 1

#define DEFAULT_GATEWAY "192.168.0.1"
#define DEFAULT_MASK "255.255.255.0"
#define DEFAULT_NTP_SERVER "ru.pool.ntp.org"

#ifndef LED_PIN
#define LED_PIN 1
#endif

#ifndef BLYNK_KEY
#define BLYNK_KEY ""
#else
#pragma message(VAR_NAME_VALUE(BLYNK_KEY))
#endif

#ifndef MQTT_HOST
#define MQTT_HOST ""
#else
#pragma message(VAR_NAME_VALUE(MQTT_HOST))
#endif

#ifndef MQTT_LOGIN
#define MQTT_LOGIN ""
#else
#pragma message(VAR_NAME_VALUE(MQTT_LOGIN))
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#else
#pragma message(VAR_NAME_VALUE(MQTT_PASSWORD))
#endif

#ifndef WATERIUS_EMAIL
#define WATERIUS_EMAIL ""
#else
#pragma message(VAR_NAME_VALUE(WATERIUS_EMAIL))
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

/**
 * @brief Параметры конфигурации по умолчанию
 */
static const uint8_t default_version PROGMEM = CURRENT_VERSION;
static const char default_waterius_name[] PROGMEM = BRAND_NAME;
static const char default_waterius_email[] PROGMEM = WATERIUS_EMAIL;
static const char default_waterius_host[] PROGMEM = WATERIUS_HOST;
static const char default_waterius_key[] PROGMEM = WATERIUS_KEY;
static const char default_blynk_host[] PROGMEM = "blynk-cloud.com";
static const char default_blynk_key[] PROGMEM = BLYNK_KEY;
static const char default_email_title[] PROGMEM = "Новые показания {DEVICE_NAME}";
static const char default_email_template[] PROGMEM = "Показания:<br>Холодная: {V1}м³(+{V4}л)<br>Горячая: {V0}м³ (+{V3}л)<hr>Питание: {V2}В<br>Resets: {V5}";
static const char default_mqtt_host[] PROGMEM = MQTT_HOST;
static const uint16_t default_mqtt_port PROGMEM = MQTT_DEFAULT_PORT;
static const char default_mqtt_login[] PROGMEM = MQTT_LOGIN;
static const char default_mqtt_password[] PROGMEM = MQTT_PASSWORD;
static const char default_discovery_topic[] PROGMEM = DISCOVERY_TOPIC;
static const bool default_mqtt_auto_discovery PROGMEM = MQTT_AUTO_DISCOVERY;
static const uint16_t default_wakeup_per_min PROGMEM = DEFAULT_WAKEUP_PERIOD_MIN;
static const uint8_t default_factor1 PROGMEM = AUTO_IMPULSE_FACTOR;
static const uint8_t default_factor0 PROGMEM = AS_COLD_CHANNEL;
static const uint32_t default_ip PROGMEM = 0;
static const uint32_t default_gateway PROGMEM = 0x0100A8C0;
static const uint32_t default_mask PROGMEM = 0x00FFFFFF;
static const char default_ntp_server[] PROGMEM = DEFAULT_NTP_SERVER;
static const char default_wifi_ssid[] PROGMEM = WIFI_SSID;
static const char default_wifi_password[] PROGMEM = WIFI_PASS;
static const char slash[] PROGMEM = "/";

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
    uint8_t version; // Версия конфигурации

    uint8_t reserved;

    // SEND_WATERIUS

    // http/https сервер для отправки данных в виде JSON
    // вид: http://host[:port][/path]
    //      https://host[:port][/path]
    char waterius_host[HOST_LEN];
    char waterius_key[WATERIUS_KEY_LEN];
    char waterius_email[EMAIL_LEN];

    // SEND_BLYNK
    // уникальный ключ устройства blynk
    char blynk_key[BLYNK_KEY_LEN];
    // сервер blynk.com или свой blynk сервер
    char blynk_host[HOST_LEN];

    // Если email не пустой, то отсылается e-mail
    // Чтобы работало нужен виджет эл. почта в приложении
    char blynk_email[EMAIL_LEN];
    // Заголовок письма. {V0}-{V4} заменяются на данные
    char blynk_email_title[BLYNK_EMAIL_TITLE_LEN];
    // Шаблон эл. письма. {V0}-{V4} заменяются на данные
    char blynk_email_template[BLYNK_EMAIL_TEMPLATE_LEN];

    char mqtt_host[HOST_LEN];
    uint16_t mqtt_port;
    char mqtt_login[MQTT_LOGIN_LEN];
    char mqtt_password[MQTT_PASSWORD_LEN];
    char mqtt_topic[MQTT_TOPIC_LEN];

    /*
    Показания счетчиках в кубометрах,
    введенные пользователем при настройке
    */
    float channel0_start;
    float channel1_start;

    /*
    Кол-во литров на 1 импульс
    */
    uint8_t factor0;
    uint8_t factor1;

    /*
    Серийные номера счётчиков воды
    */
    char serial0[SERIAL_LEN];
    char serial1[SERIAL_LEN];

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
    Время последнего пробуждения
    */
    uint32_t wake_time;

    /*
    За сколько времени настроили ватериус
    */
    uint32_t setup_time;

    /*
    Статический адрес
    */
    uint32_t ip;
    uint32_t gateway;
    uint32_t mask;

    /*
    Период пробуждение для отправки данных, мин
    */
    uint16_t wakeup_per_min;

    /*
    Установленный период отправки с учетом погрешности
    */
    uint16_t set_wakeup;

    /*
    Время последней отправки по расписанию
    */
    time_t last_send; // Size of time_t: 8

    /*
    Режим пробуждения
    */
    uint8_t mode; // SETUP_MODE

    /*
    Успешная настройка
    */
    uint8_t setup_finished_counter;

    /* Публиковать данные для автоматического добавления в Homeassistant */
    uint8_t mqtt_auto_discovery;
    uint8_t reserved2;

    /* Топик MQTT*/
    char mqtt_discovery_topic[MQTT_TOPIC_LEN];

    /* пользовательский NTP сервер */
    char ntp_server[HOST_LEN];

    /* имя сети Wifi */
    char wifi_ssid[WIFI_SSID_LEN];
    /* пароль к Wifi сети */
    char wifi_password[WIFI_PWD_LEN];
    /* mac сети Wifi */
    uint8_t wifi_bssid[6];
    /* Wifi канал */
    uint8_t wifi_channel;
    uint8_t reserved3;    // выравниваем по границе

    /*
    Зарезервируем кучу места, чтобы не писать конвертер конфигураций.
    Будет актуально для On-the-Air обновлений
    */
    uint8_t reserved4[64];

}; // 960 байт

#endif