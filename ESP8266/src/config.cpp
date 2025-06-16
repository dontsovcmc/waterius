
#include "config.h"
#include "Logging.h"

#include <ESP8266WiFi.h>
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"
#include "porting.h"
#include "sync_time.h"
#include "wifi_helpers.h"
#include "flash_hal.h"


// Конвертируем значение переменных компиляции в строк
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "=" VALUE(var)

// Сохраняем конфигурацию в EEPROM
void store_config(const Settings &sett)
{
    uint16_t crc = get_checksum(sett);
    EEPROM.begin(sizeof(sett) + sizeof(crc));
    EEPROM.put(0, sett);
    EEPROM.put(sizeof(sett), crc);

    if (!EEPROM.commit())
    {
        LOG_ERROR(F("Config stored FAILED"));
    }
    else
    {
        LOG_INFO(F("Config stored OK crc=") << crc);
    }
    EEPROM.end();
}

// Инициализация параметров по умолчанию
bool init_config(Settings &sett)
{   
    sett.version = CURRENT_VERSION;
    LOG_INFO(F("cfg version=") << sett.version);

    sett.wakeup_per_min = DEFAULT_WAKEUP_PERIOD_MIN;
    sett.period_min_tuned = DEFAULT_WAKEUP_PERIOD_MIN;
    sett.mode = SETUP_MODE;
    sett.mqtt_auto_discovery = (uint8_t)MQTT_AUTO_DISCOVERY;
    
    sett.counter0_name = CounterName::WATER_HOT;
    sett.counter1_name = CounterName::WATER_COLD;

    sett.factor0 = AS_COLD_CHANNEL;
    sett.factor1 = AUTO_IMPULSE_FACTOR;

    sett.waterius_on = (uint8_t)true;
    sett.http_on = (uint8_t)false;
    sett.mqtt_on = (uint8_t)false;
    sett.dhcp_off = (uint8_t)false;

    //можно оптимизировать и загружать из PROGMEM, но ради 2х полей смысла не вижу
    //static const char WATERIUS_DEFAULT_DOMAIN[] PROGMEM =  "https://cloud.waterius.ru"
    //strncpy_P(sett.waterius_host, WATERIUS_DEFAULT_DOMAIN, HOST_LEN);

    strncpy0(sett.waterius_host, WATERIUS_DEFAULT_DOMAIN, sizeof(WATERIUS_DEFAULT_DOMAIN));

    String default_topic = String(MQTT_DEFAULT_TOPIC_PREFIX) + "/" + String(getChipId()) + "/";
    strncpy0(sett.mqtt_topic, default_topic.c_str(), default_topic.length() + 1);
    
    String discovery_topic(DISCOVERY_TOPIC);
    strncpy0(sett.mqtt_discovery_topic, discovery_topic.c_str(), discovery_topic.length() + 1);

    strncpy0(sett.ntp_server, DEFAULT_NTP_SERVER, sizeof(DEFAULT_NTP_SERVER));

    IPAddress network_gateway;
    network_gateway.fromString(DEFAULT_GATEWAY);
    sett.gateway = network_gateway;
    
    IPAddress network_mask;
    network_mask.fromString(DEFAULT_MASK);
    sett.mask = network_mask;

    // Можно задать константы при компиляции, чтобы Ватериус сразу заработал

#ifdef WATERIUS_HOST
#pragma message(VAR_NAME_VALUE(WATERIUS_HOST))
    String waterius_host = VALUE(WATERIUS_HOST);
    strncpy0(sett.waterius_host, waterius_host.c_str(), WATERIUS_HOST_LEN);
    LOG_INFO("default waterius_host=" << waterius_host);
#endif

#ifdef MQTT_HOST
#pragma message(VAR_NAME_VALUE(MQTT_HOST))
    String mqtt_host = VALUE(MQTT_HOST);
    strncpy0(sett.mqtt_host, mqtt_host.c_str(), HOST_LEN);
    LOG_INFO("default mqtt_host=" << mqtt_host);
#endif

#ifdef MQTT_LOGIN
#pragma message(VAR_NAME_VALUE(MQTT_LOGIN))
    String mqtt_login = VALUE(MQTT_LOGIN);
    strncpy0(sett.mqtt_login, mqtt_login.c_str(), MQTT_LOGIN_LEN);
    LOG_INFO("default mqtt_login=" << mqtt_login);
#endif

#ifdef MQTT_PASSWORD
#pragma message(VAR_NAME_VALUE(MQTT_PASSWORD))
    String mqtt_password = VALUE(MQTT_PASSWORD);
    strncpy0(sett.mqtt_password, mqtt_password.c_str(), MQTT_PASSWORD_LEN);
    LOG_INFO("default mqtt_password=" << mqtt_password);
#endif

#ifdef WATERIUS_EMAIL
#pragma message(VAR_NAME_VALUE(WATERIUS_EMAIL))
    strncpy0(sett.waterius_email, VALUE(WATERIUS_EMAIL), EMAIL_LEN);
    LOG_INFO(F("default waterius email=") << VALUE(WATERIUS_EMAIL));
#endif

    LOG_INFO(F("Generate waterius key"));
    generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, sett.waterius_email);
    LOG_INFO(F("waterius key=") << sett.waterius_key);

#ifdef WIFI_SSID
#pragma message(VAR_NAME_VALUE(WIFI_SSID))
#ifdef WIFI_PASS
#pragma message(VAR_NAME_VALUE(WIFI_PASS))
    strncpy0(sett.wifi_ssid, VALUE(WIFI_SSID), WIFI_SSID_LEN);
    strncpy0(sett.wifi_password, VALUE(WIFI_PASS), WIFI_PWD_LEN);
    return true;
#endif
#endif
    return false;
}

/* Загружаем конфигурацию в EEPROM. true - успех. */
bool load_config(Settings &sett)
{
    LOG_INFO(F("Loading Config..."));
    uint16_t crc = 0;
    Settings tmp_sett = {};
    EEPROM.begin(sizeof(tmp_sett) + sizeof(crc)); //  4 до 4096 байт. с адреса 0x7b000.
    EEPROM.get(0, tmp_sett);
    EEPROM.get(sizeof(tmp_sett), crc);
    EEPROM.end();

    uint16_t calculated_crc = get_checksum(tmp_sett);
    if (crc == calculated_crc)
    {
        if (tmp_sett.version != sett.version)
        {
            LOG_INFO(F("ESP has old configuration version=") << tmp_sett.version);
            LOG_INFO(F("Init configuration version=") << sett.version);
            bool ret = init_config(sett);
            
            strncpy0(sett.waterius_key, tmp_sett.waterius_key, WATERIUS_KEY_LEN);
            LOG_INFO(F("Restore waterius_key=") << sett.waterius_key);
            store_config(sett);

            return ret;
        }
        else 
        {
            sett = tmp_sett;
            LOG_INFO(F("Configuration CRC ok"));

            // Для безопасной работы с буферами,  в библиотеках может не быть проверок
            sett.waterius_host[HOST_LEN - 1] = 0;
            sett.waterius_key[WATERIUS_KEY_LEN - 1] = 0;
            sett.waterius_email[EMAIL_LEN - 1] = 0;

            sett.http_url[HOST_LEN - 1] = 0;

            sett.mqtt_host[HOST_LEN - 1] = 0;
            sett.mqtt_login[MQTT_LOGIN_LEN - 1] = 0;
            sett.mqtt_password[MQTT_PASSWORD_LEN - 1] = 0;
            sett.mqtt_topic[MQTT_TOPIC_LEN - 1] = 0;
            sett.mqtt_discovery_topic[MQTT_TOPIC_LEN - 1] = 0;

            sett.ntp_server[HOST_LEN - 1] = 0;

            LOG_INFO(F("wakeup min=") << sett.wakeup_per_min);

            LOG_INFO(F("--- Waterius.ru ---- "));
            if (sett.waterius_on) {
                LOG_INFO(F("state=ON"));
            } else {
                LOG_INFO(F("state=OFF"));
            }
            LOG_INFO(F("email=") << sett.waterius_email);
            LOG_INFO(F("host=") << sett.waterius_host << F(" key=") << sett.waterius_key);
            LOG_INFO(F("place=") << sett.place);
            LOG_INFO(F("company=") << sett.company);

            LOG_INFO(F("--- HTTP ---- "));
            if (sett.http_on) {
                LOG_INFO(F("state=ON"));
            } else {
                LOG_INFO(F("state=OFF"));
            }
            LOG_INFO(F("host=") << sett.http_url);

            LOG_INFO(F("--- MQTT ---- "));
            if (sett.mqtt_on) {
                LOG_INFO(F("state=ON"));
            } else {
                LOG_INFO(F("state=OFF"));
            }
            LOG_INFO(F("host=") << sett.mqtt_host << F(" port=") << sett.mqtt_port);
            LOG_INFO(F("login=") << sett.mqtt_login << F(" pass=") << sett.mqtt_password);
            LOG_INFO(F("auto discovery=") << sett.mqtt_auto_discovery);
            LOG_INFO(F("discovery topic=") << sett.mqtt_discovery_topic);

            LOG_INFO(F("--- Network ---- "));
            if (sett.ip)
            {
                if (sett.dhcp_off) {
                    LOG_INFO(F("DHCP=OFF"));
                } else {
                    LOG_INFO(F("DHCP=ON"));
                }
                LOG_INFO(F("static_ip=") << IPAddress(sett.ip).toString());
                LOG_INFO(F("gateway=") << IPAddress(sett.gateway).toString());
                LOG_INFO(F("mask=") << IPAddress(sett.mask).toString());
            }
            else
            {
                LOG_INFO(F("DHCP is on"));
            }

            LOG_INFO(F("ntp_server=") << sett.ntp_server);
            LOG_INFO(F("ntp_error_counter=") << sett.ntp_error_counter);

            LOG_INFO(F("--- WIFI ---- "));
            LOG_INFO(F("wifi_ssid=") << sett.wifi_ssid);
            LOG_INFO(F("wifi_channel=") << sett.wifi_channel);
            LOG_INFO(F("wifi_phy_mode=") << wifi_phy_mode_title((WiFiPhyMode_t)sett.wifi_phy_mode));
            LOG_INFO(F("wifi_connect_errors=") << sett.wifi_connect_errors);
            LOG_INFO(F("wifi_connect_attempt=") << sett.wifi_connect_attempt);

            LOG_INFO(F("Config succesfully loaded"));
            return true;
        }
    }
    else
    {
        LOG_INFO(F("ESP config CRC failed. Maybe first run. Init configuration."));
        LOG_INFO(F("Saved crc=") << crc << F(" calculated=") << calculated_crc);
        return init_config(sett);
    }
}

/*
Берем начальные показания и кол-во импульсов,
вычисляем текущие показания по новому кол-ву импульсов
*/
void calculate_values(Settings &sett, const SlaveData &data, CalculatedData &cdata)
{
    LOG_INFO(F("Calculating values..."));

    if (sett.factor0 > 0) 
    {
        if (data.impulses0 < sett.impulses0_start) {
            sett.impulses0_start = data.impulses0;
            // Лучше потеряем в точности, чем будет показания миллионы
            LOG_ERROR(F("Impulses0 less than start. Reset impulses0_start"));
        }

        if (sett.counter0_name == CounterName::ELECTRO)
        {
            // factor0 кол-во импульсов на 1 кВт * ч
            cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / (sett.factor0 * 1.0);
            cdata.delta0 = (data.impulses0 - sett.impulses0_previous) / (sett.factor0 * 1.0);
        }
        else 
        {
            // factor0 кол-во литров на 1 импульс, переводим в кубометры
            cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.factor0;
            cdata.delta0 = (data.impulses0 - sett.impulses0_previous) * sett.factor0;
        }
    }

    LOG_INFO(F("channel0  name=") << sett.counter0_name);
    LOG_INFO(F("impulses0 start=") << sett.impulses0_start << F(" current=") << data.impulses0 << F(" factor=") << sett.factor0);
    LOG_INFO(F("value0    start=") << sett.channel0_start << F(" current=") << cdata.channel0 << F(" delta=") << cdata.delta0);

    if (sett.factor1 > 0) 
    {
        if (data.impulses1 < sett.impulses1_start) {
            sett.impulses1_start = data.impulses1;
            LOG_ERROR(F("Impulses1 less than start. Reset impulses1_start"));
        }

        if (sett.counter1_name == CounterName::ELECTRO)
        {
            // factor1 кол-во импульсов на 1 кВт * ч
            cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / (sett.factor1 * 1.0);
            cdata.delta1 = (data.impulses1 - sett.impulses1_previous) / (sett.factor1 * 1.0);
        }
        else
        {
            // factor1 кол-во литров на 1 импульс, переводим в кубометры
            cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.factor1;
            cdata.delta1 = (data.impulses1 - sett.impulses1_previous) * sett.factor1;
        }
    }

    LOG_INFO(F("channel1  name=") << sett.counter1_name);
    LOG_INFO(F("impulses1 start=") << sett.impulses1_start << F(" current=") << data.impulses1 << F(" factor=") << sett.factor1);
    LOG_INFO(F("value1    start=") << sett.channel1_start << F(" current=") << cdata.channel1 << F(" delta=") << cdata.delta1);
}

/*
 * @brief Корректируем период пробуждения только для автоматического режима.
 *
 * @param const time_t &now - Полученное по NTP время. Должно быть валидным.
 * @param const time_t &base_time - Время настройки пользователем
 * @param const time_t &last_send - Предыдущее время пробуждения
 * @param const uint16_t &wakeup_per_min - Установленный пользователем период пробуждения
 * @param const uint16_t &period_min_tuned - Скорректированный период пробуждения
 *
 * @returns uint16_t - Кол-во минут ("кривых attiny"), через которое должно проснуться устройство.
 */
uint16_t tune_wakeup(const time_t &now, const time_t &base_time, const time_t &last_send, 
    const uint16_t &wakeup_per_min, const uint16_t &period_min_tuned)
{
    // 1. Оценка коэффициента 'k' на основе результатов последнего сна
    // time_t обычно хранит секунды, поэтому difftime вернет разницу в секундах.
    double actual_slept_min = difftime(now, last_send) / 60.0;

    double k_estimated = 1.0;
    if (period_min_tuned > 0) {
        k_estimated = actual_slept_min / period_min_tuned;
    }

    // 2. Определение следующей целевой временной отметки
    double time_since_base_min = difftime(now, base_time) / 60.0;
    
    // Целочисленное деление для определения количества прошедших периодов
    long long target_num = static_cast<long long>(floor(time_since_base_min / wakeup_per_min)) + 1;
    
    time_t next_expected = base_time + target_num * wakeup_per_min * 60;
    double minutes_to_next = difftime(next_expected, now) / 60.0;

    // 3. Защита от "ловушки": если до цели меньше минуты или до пробуждения меньше 5% периода, целимся в следующую точку
    if (minutes_to_next < 1.0 || minutes_to_next < (wakeup_per_min * 0.05)) {
        target_num++;
        next_expected = base_time + target_num * wakeup_per_min * 60;
        minutes_to_next = difftime(next_expected, now) / 60.0;
    }

    // 4. Расчет и округление нового периода сна
    double ideal_period_tuned_float = minutes_to_next;
    if (k_estimated > 0.1) { // Проверка, что k_estimated не слишком близок к нулю
        ideal_period_tuned_float = minutes_to_next / k_estimated;
    }

    uint16_t new_period_min_tuned = static_cast<uint16_t>(round(ideal_period_tuned_float));
    LOG_INFO(F("Tune: elapsed_min=") << actual_slept_min << ", new_period_min_tuned=" << new_period_min_tuned);

    // Округляем до ближайшего целого и приводим к целевому типу uint16_t
    return new_period_min_tuned;
}

/* Сбрасываем скорректированный период после изменения периода пользователем */
void reset_period_min_tuned(Settings &sett)
{
    sett.period_min_tuned = sett.wakeup_per_min * 0.9;   
    LOG_INFO(F("RESET: period_min_tuned=") << sett.period_min_tuned);

}

/* Обновляем значения в конфиге */
void update_config(Settings &sett, const SlaveData &data, const CalculatedData &cdata)
{
    LOG_INFO(F("Updating config..."));
    // Сохраним текущие значения в памяти.
    sett.impulses0_previous = data.impulses0;
    sett.impulses1_previous = data.impulses1;

    // Значение по умолчанию. Уменьшим с учётом опроса входов.
    if (!sett.period_min_tuned) {
        reset_period_min_tuned(sett);
    }

    // Перешлем время пробуждения на сервер при след. включении
    sett.wake_time = millis();

    time_t now = time(nullptr);

    // Проверяем валидность текущего времени
    if (!is_valid_time(now)) 
    {
        LOG_ERROR(F("Invalid current time!"));
        return;
    }

    // Обновляем базовое время при ручном пробуждении или первом запуске
    if (!is_valid_time(sett.base_time) || sett.mode == MANUAL_TRANSMIT_MODE || sett.mode == SETUP_MODE) 
    {
        sett.base_time = now;
        LOG_INFO(F("Manual/first wakeup: base_time reset"));
    }

    LOG_INFO(F("Wakeup period, min:") << sett.wakeup_per_min);

    // Корректируем период пробуждения только для автоматического режима
    if (sett.mode == TRANSMIT_MODE)
    {    
        sett.period_min_tuned = tune_wakeup(now, sett.base_time, sett.last_send, sett.wakeup_per_min, sett.period_min_tuned);
    }

    // Обновляем метку последней активности
    sett.last_send = now;
}

void factory_reset(Settings &sett)
{
    //Запоминаем уникальный токен, чтобы потом восстановить
    LOG_INFO(F("Save waterius_key=") << sett.waterius_key);
    String waterius_key = sett.waterius_key;

    ESP.eraseConfig();
    delay(100);
    LOG_INFO(F("EEPROM erased"));

    // The flash cache maps the physical flash into the address space at offset  \ FS_PHYS_ADDR - ?
    ESP.flashEraseSector(((EEPROM_start - 0x40200000) / SPI_FLASH_SEC_SIZE));
    LOG_INFO(F("0x40200000 erased"));

    delay(500);

    init_config(sett);
    strncpy0(sett.waterius_key, waterius_key.c_str(), WATERIUS_KEY_LEN);
    LOG_INFO(F("Restore waterius_key=") << sett.waterius_key);
    store_config(sett);

    delay(500);

    ESP.reset();
}