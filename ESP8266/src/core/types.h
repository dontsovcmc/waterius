#ifndef _WATERIUS_CORE_TYPES_h
#define _WATERIUS_CORE_TYPES_h

/*
Модель данных прошивки: структуры, перечисления и константы, которые их
описывают.

Файл — часть чистого ядра src/core: никакого Arduino.h, String, EEPROM и
работы с железом. Ядро собирается и в прошивку, и в хостовые тесты
(pio test -d ESP8266 -e native), поэтому всё, что тянет Arduino, должно
оставаться в src/ рядом с адаптерами.
*/

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/*
Версия структуры настроек в EEPROM
*/
#define VER_8 8
#define VER_9 9
#define VER_10 10
#define VER_11 11
#define VER_12 12
#define CURRENT_VERSION VER_11

/*
Длины полей настроек
*/
#define EMAIL_LEN 40

#define WATERIUS_KEY_LEN 34
#define HOST_LEN 64

#define COMPANY_LEN 20
#define PLACE_LEN 40
#define BLYNK_RESERVED 38

#define MQTT_LOGIN_LEN 32
#define MQTT_PASSWORD_LEN 66 //ansible образ home assistant генерирует пароль длиной 64
#define MQTT_TOPIC_LEN 64

#define MQTT_DEFAULT_PORT 1883

#ifndef DISCOVERY_TOPIC
#define DISCOVERY_TOPIC "homeassistant"
#endif

#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY true // если true то публикуется автодискавери топик для Home Assistant
#endif

#define SERIAL_LEN 16

#define WIFI_SSID_LEN 32
#define WIFI_PWD_LEN 64

#define CHANNEL_NUM 2

/*
Период пробуждения по умолчанию, мин
*/
#ifndef DEFAULT_WAKEUP_PERIOD_MIN
#define DEFAULT_WAKEUP_PERIOD_MIN 1440
#endif

/*
Спец. значения веса импульса
*/
#define AUTO_IMPULSE_FACTOR 3
#define AS_COLD_CHANNEL 7

/*
Модель устройства. Задаётся флагом компиляции -DWATERIUS_MODEL, приходит
от attiny85 в поле AttinyData.model.
*/
#define WATERIUS_MODEL_1 0
#define WATERIUS_MODEL_2 2

/*
Флаги состояния из AttinyData.attiny_flags.

ESP_POWERED_LONG: attiny держит питание ЕСП дольше нескольких секунд.

Байт появился в версии ATTINY_VER_POWER_FLAGS: до неё на его месте лежал
резерв, всегда нулевой, но полагаться на это нельзя - значение проверяется
по версии прошивки attiny. Порог назван по содержимому байта, а не по
конкретному флагу: следующие флаги лягут в этот же байт.
*/
#define ATTINY_FLAG_ESP_POWERED_LONG 0x01
#define ATTINY_VER_POWER_FLAGS 40

/*
Режим пробуждения, приходит от attiny85
*/
#define SETUP_MODE 1
#define TRANSMIT_MODE 2
#define MANUAL_TRANSMIT_MODE 3

/*
    Статус обновления прошивки
 */
enum OtaError
{
    OTA_ERR_NONE = 0,
    OTA_ERR_PARSE = 1,
    OTA_ERR_FS_UPDATE = 2,
    OTA_ERR_FW_UPDATE = 3,
    OTA_ERR_LOW_BATTERY = 4
};

/*
   Вход attiny
 */
enum InputColor
{
    INPUT0_RED = 0,  // 0 - Красный вход, ГВС
    INPUT1_BLUE = 1  // 1 - Синий вход, ХВС
};


enum CounterType
{
    NAMUR = 0,
    DISCRETE = 1,
    ELECTRONIC = 2,
    HALL = 3,
    ELECTRONIC_HIGH = 4,   // электронный выход, импульс — подъём линии (issue #379)
    NONE = 0xFF   // 255
};


enum CounterName
{
    WATER_COLD = 0,
    WATER_HOT = 1,
    ELECTRO = 2,
    GAS = 3,
    HEAT_GCAL = 4,
    PORTABLE_WATER = 5,
    OTHER = 6,
    HEAT_KWT = 7
};


// согласно
enum DataType
{
    COLD_WATER = 0,
    HOT_WATER = 1,
    ELECTRICITY = 2,
    GAS_DATA = 3,
    HEATING_GCAL = 4,
    ELECTRICITY_DAY = 5,
    ELECTRICITY_NIGHT = 6,
    ELECTRICITY_PEAK = 7,
    ELECTRICITY_HALF_PEAK = 8,
    POTABLE_WATER = 9,
    OTHER_TYPE = 10,
    ELECTRICITY_TOTAL = 11,  // not used here
    HEATING_KWT = 12
};

/*
Данные принимаемые от Attiny
*/
struct AttinyData
{
    // Header
    uint8_t version;    // Версия ПО Attiny
    uint8_t service;    // Причина загрузки Attiny (биты 0-5), биты 6-7 = on_pulse флаги
    bool on_pulse0;
    bool on_pulse1;
    uint8_t reserved5 = 0;
    uint16_t voltage;   // Напряжение питания в мВ
    uint8_t attiny_flags = 0;  // Флаги состояния attiny, см. ATTINY_FLAG_*
    uint8_t setup_started_counter;
    uint8_t resets;
    uint8_t model;         // WATERIUS_MODEL_1 или  WATERIUS_MODEL_2
    uint8_t counter_type0; // Тип входа, вход 0
    uint8_t counter_type1; //           вход 1
    uint32_t impulses0;    // Импульсов, канал 0
    uint32_t impulses1;    //           канал 1
    uint16_t adc0;         // Уровень,   канал 0
    uint16_t adc1;         //           канал 1

    // HEADER_DATA_SIZE

    uint8_t crc = 0; // Всегда в конце структуры данных
    uint8_t reserved2 = 0;
    // Кратно 16bit https://github.com/esp8266/Arduino/issues/1825
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

Размер структуры:
На ESP8266 (и большинстве 32-битных платформ) выравнивание по 2 байта обычно означает,
что все поля должны начинаться с адреса, кратного 2.

Но если в структуре встречаются поля большего размера (например, time_t — 8 байт),
компилятор может выравнивать их по 4 или 8 байтам, чтобы ускорить доступ к данным.

Если перед time_t идут поля с меньшим выравниванием (например, char или uint8_t),
компилятор может вставить дополнительные байты-паддинги для правильного выравнивания следующего поля.
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
    char company[COMPANY_LEN] = {0};
    char place[PLACE_LEN] = {0};

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
    Статистика подключений к Wi-Fi
    */
    uint8_t wifi_connect_errors = 0;
    uint8_t wifi_connect_attempt = 0;

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
    uint16_t period_min_tuned = DEFAULT_WAKEUP_PERIOD_MIN;

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

    /*
    Неудачные попытки синхронизации подряд: обнуляется первым же успехом.
    По нему считается пауза перед следующей попыткой (2^N пробуждений),
    поэтому здесь именно серия, а не сумма за всё время. Уходит на сервер
    как ntp_errors.
    */
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

    /* Код ошибки обновления прошивки */
    uint8_t ota_error = OTA_ERR_NONE;

    /* Включение DHCP или статических настроек */
    uint8_t dhcp_off = (uint8_t) false;

    /* Retain сообщения MQTT */
    uint8_t mqtt_retain = (uint8_t)true;

    time_t base_time = 0; // Size of time_t: 8

    /*
    Поправочный коэффициент для voltage Attiny в процентах (100 = без коррекции)
    */
    uint8_t voltage_cal = 100;

    /*
    Сколько раз время уже синхронизировалось. Считаем только первые
    NTP_WARMUP_SYNCS: пока пары синхронизаций нет, поправку частоты attiny
    измерить не по чему, поэтому такие пробуждения синхронизируются подряд.

    Поле занимает бывший reserved8 — байт, который у прошитых устройств
    лежит нулём, так что они пройдут прогрев заново.
    */
    uint8_t ntp_sync_count = 0;

    /*
    Время последней успешной синхронизации по NTP и число пробуждений с тех
    пор. Пара нужна, чтобы между синхронизациями оценивать время, а на
    синхронизации — точно знать, сколько периодов проспали.

    Поля взяты из reserved9, поэтому размещение остальных полей не поехало:
    у прошитых устройств конфигурация в EEPROM читается как прежде, а новые
    поля приезжают нулями — время неизвестно, синхронизируемся сразу.
    */
    time_t last_time_sync = 0; // Size of time_t: 8
    uint16_t wakeups_since_sync = 0;

    /*
    Целый период пробуждения с поправкой на частоту attiny, в "кривых
    минутах". В отличие от period_min_tuned не зависит от того, куда
    целились в прошлый раз, поэтому годится для ручного пробуждения:
    кнопка задаёт расписание заново (#380).
    */
    uint16_t period_min_full = 0;

    /*
    Сколько минут прошло с последнего выхода на связь (#361).

    Нужен для режима "только при расходе": по нему видно, что пора выйти в
    эфир просто ради отметки "я жив", даже если воду не трогали. Считается в
    минутах, а не в пробуждениях, чтобы не зависеть от периода: смена периода
    пользователем не должна сдвигать этот срок.
    */
    uint16_t minutes_since_send = 0;

    /*
    Выходить на связь только при расходе воды (#361).

    Период пробуждения при этом не меняется: устройство просыпается как
    обычно и читает attiny, но Wi-Fi включает, только если импульсы пошли
    или пора отметиться. Однобайтовый флаг стоит последним, чтобы
    компилятор не вставил байт выравнивания и не съел резерв зря.
    */
    uint8_t send_on_consumption = 0;

    /*
    Зарезервируем кучу места, чтобы не писать конвертер конфигураций.
    Будет актуально для On-the-Air обновлений
    */
    uint8_t reserved9[53] = {0};

}; // 960 байт

/*
Размер конфигурации зафиксирован: она лежит в EEPROM как есть, и сдвиг любого
поля означает, что все прошитые устройства прочитают чужие байты. Проверка
здесь, а не в main.cpp, чтобы её видели и хостовые тесты.
*/
static_assert(sizeof(Settings) == 960, "sizeof Settings != 960");

#endif
