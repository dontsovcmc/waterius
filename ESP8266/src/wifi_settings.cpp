
#include "wifi_settings.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
#include <ESP8266WiFi.h>
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"
#include "WateriusHttps.h"

#include "porting.h"

//Конвертируем значение переменных компиляции в строк
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)

/* Сохраняем конфигурацию в EEPROM */
void storeConfig(const Settings &sett) 
{
    EEPROM.begin(sizeof(sett));
    EEPROM.put(0, sett);
    
    if (!EEPROM.commit())
    {
        //LOG_ERROR("CFG", "Config stored FAILED");
        log_cfg.err(F("Config stored FAILED"));
    }
    else
    {
        //LOG_INFO("CFG", "Config stored OK");
        log_cfg.info(F("Config stored OK"));
    }
    EEPROM.end();
}

/* Загружаем конфигурацию в EEPROM. true - успех. */
bool loadConfig(struct Settings &sett) 
{
    EEPROM.begin(sizeof(sett));  //  4 до 4096 байт. с адреса 0x7b000.
    EEPROM.get(0, sett);
    EEPROM.end();

    if (sett.crc == FAKE_CRC)  // todo: сделать нормальный crc16
    {
        //LOG_INFO(FPSTR(S_CFG), F("Configuration CRC ok"));
        log_cfg.info(F("Configuration CRC ok"));

        // Для безопасной работы с буферами,  в библиотеках может не быть проверок
        sett.waterius_host[WATERIUS_HOST_LEN-1] = '\0';
        sett.waterius_key[WATERIUS_KEY_LEN-1] = '\0';
        sett.waterius_email[EMAIL_LEN-1] = '\0';

        sett.blynk_key[BLYNK_KEY_LEN-1] = '\0';
        sett.blynk_host[BLYNK_HOST_LEN-1] = '\0';
        sett.blynk_email[EMAIL_LEN-1] = '\0';
        sett.blynk_email_title[BLYNK_EMAIL_TITLE_LEN-1] = '\0';
        sett.blynk_email_template[BLYNK_EMAIL_TEMPLATE_LEN-1] = '\0'; 

        sett.mqtt_host[MQTT_HOST_LEN-1] = '\0'; 
        sett.mqtt_login[MQTT_LOGIN_LEN-1] = '\0'; 
        sett.mqtt_password[MQTT_PASSWORD_LEN-1] = '\0'; 
        sett.mqtt_topic[MQTT_TOPIC_LEN-1] = '\0'; 

        //LOG_INFO(FPSTR(S_CFG), F("--- Waterius.ru ---- "));
        //LOG_INFO(FPSTR(S_CFG), F("email=") << sett.waterius_email);
        //LOG_INFO(FPSTR(S_CFG), F("host=") << sett.waterius_host << F(" key=") << sett.waterius_key);
        //LOG_INFO(FPSTR(S_CFG), F("wakeup min=") << sett.wakeup_per_min);
        log_cfg.info(F("--- Waterius.ru ---- "));
        log_cfg.info(F("email=%s"), sett.waterius_email);
        log_cfg.info(F("host=%s key=%s"), sett.waterius_host, sett.waterius_key);
        log_cfg.info(F("wakeup min=%i"), sett.wakeup_per_min);

        
        //LOG_INFO(FPSTR(S_CFG), F("--- Blynk.cc ---- "));
        //LOG_INFO(FPSTR(S_CFG), F("host=") << sett.blynk_host << F(" key=") << sett.blynk_key);
        //LOG_INFO(FPSTR(S_CFG), F("email=") << sett.blynk_email);
        log_cfg.info(F("--- Blynk.cc ---- "));
        log_cfg.info(F("host=%s key=%s"), sett.blynk_host, sett.blynk_key);
        log_cfg.info(F("email=%s"), sett.blynk_email);
        

        //LOG_INFO(FPSTR(S_CFG), F("--- MQTT ---- "));
        //LOG_INFO(FPSTR(S_CFG), F("host=") << sett.mqtt_host << F(" port=") << sett.mqtt_port);
        //LOG_INFO(FPSTR(S_CFG), F("login=") << sett.mqtt_login << F(" pass=") << sett.mqtt_password);
        //LOG_INFO(FPSTR(S_CFG), F("topic=") << sett.mqtt_topic);   
        log_cfg.info(F("--- MQTT ---- "));
        log_cfg.info(F("host=%s port=%i"), sett.mqtt_host, sett.mqtt_port);
        log_cfg.info(F("login=%s pass=%s"), sett.mqtt_login, sett.mqtt_password);
        log_cfg.info(F("topic=%s"), sett.mqtt_topic);
        
        //LOG_INFO(FPSTR(S_CFG), F("--- Network ---- "));
        log_cfg.info(F("--- Network ---- "));
        if (sett.ip) {
            //LOG_INFO(FPSTR(S_CFG), F("DHCP turn off"));
            log_cfg.info(F("DHCP turn off"));
        } else {
            //LOG_INFO(FPSTR(S_CFG), F("DHCP is on"));
            log_cfg.info(F("DHCP is on"));
        }
        //LOG_INFO(FPSTR(S_CFG), F("static_ip=") << IPAddress(sett.ip).toString());
        //LOG_INFO(FPSTR(S_CFG), F("gateway=") << IPAddress(sett.gateway).toString());
        //LOG_INFO(FPSTR(S_CFG), F("mask=") << IPAddress(sett.mask).toString());
        log_cfg.info(F("static_ip="), IPAddress(sett.ip).toString().c_str());
        log_cfg.info(F("gateway="), IPAddress(sett.gateway).toString().c_str());
        log_cfg.info(F("mask="), IPAddress(sett.mask).toString().c_str());

        // Всегда одно и тоже будет
        //LOG_INFO(FPSTR(S_CFG), F("--- Counters ---- "));
        //LOG_INFO(FPSTR(S_CFG), F("channel0 start=") << sett.channel0_start << F(", impulses=") << sett.impulses0_start << F(", factor=") << sett.factor0);
        //LOG_INFO(FPSTR(S_CFG), F("channel1 start=") << sett.channel1_start << F(", impulses=") << sett.impulses1_start << F(", factor=") << sett.factor1);
        log_cfg.info(F("--- Counters ---- "));
        log_cfg.info(F("channel%i start=%f, impulses=%i, factor=%i"), 0, sett.channel0_start, sett.impulses0_start, sett.factor0);
        log_cfg.info(F("channel%i start=%f, impulses=%i, factor=%i"), 1, sett.channel1_start, sett.impulses1_start, sett.factor1);
        return true;

    } else {    
        // Конфигурация не была сохранена в EEPROM, инициализируем с нуля
        //LOG_INFO(FPSTR(S_CFG), F("Configuration CRC failed=") << sett.crc );
        log_cfg.info(F("Configuration CRC failed=%i"), sett.crc );
        
        // Заполняем нулями всю конфигурацию
        memset(&sett, 0, sizeof(sett));

        sett.version = CURRENT_VERSION;  //для совместимости в будущем
        //LOG_INFO(FPSTR(S_CFG), F("version=") << sett.version);
        log_cfg.info(F("version=%i"), sett.version);

        strncpy0(sett.waterius_host, WATERIUS_DEFAULT_DOMAIN, WATERIUS_HOST_LEN);

        strncpy0(sett.blynk_host, BLYNK_DEFAULT_DOMAIN, BLYNK_HOST_LEN);

        String email_title = F("Новые показания {DEVICE_NAME}");
        strncpy0(sett.blynk_email_title, email_title.c_str(), BLYNK_EMAIL_TITLE_LEN);

        String email_template = F("Показания:<br>Холодная: {V1}м³(+{V4}л)<br>Горячая: {V0}м³ (+{V3}л)<hr>Питание: {V2}В<br>Resets: {V5}");
        strncpy0(sett.blynk_email_template, email_template.c_str(), BLYNK_EMAIL_TEMPLATE_LEN);

        //strncpy0(sett.mqtt_host, MQTT_DEFAULT_HOST, MQTT_HOST_LEN);
        String defaultTopic = String(MQTT_DEFAULT_TOPIC_PREFIX) + String(getChipId()) + "/";

        strncpy0(sett.mqtt_topic, defaultTopic.c_str(), MQTT_TOPIC_LEN);
        sett.mqtt_port = MQTT_DEFAULT_PORT;
        
        sett.gateway = IPAddress(192,168,0,1);
        sett.mask = IPAddress(255,255,255,0);

        sett.factor1 = AUTO_IMPULSE_FACTOR; 
        sett.factor0 = AS_COLD_CHANNEL;

        sett.wakeup_per_min = DEFAULT_WAKEUP_PERIOD_MIN;
        sett.SaveLogLevel = -1;
        
//Можно задать константы при компиляции, чтобы Ватериус сразу заработал

#ifdef BLYNK_KEY    
        #pragma message(VAR_NAME_VALUE(BLYNK_KEY))
        String key = VALUE(BLYNK_KEY);
        strncpy0(sett.blynk_key, key.c_str(), BLYNK_KEY_LEN);
        //LOG_INFO(FPSTR(S_CFG), F("default Blynk key=") << key);
        log_cfg.info(F("default Blynk key=%s"), key.c_str());
#endif

#ifdef WATERIUS_HOST
        #pragma message(VAR_NAME_VALUE(WATERIUS_HOST))
        String waterius_host = VALUE(WATERIUS_HOST);
        strncpy0(sett.waterius_host, waterius_host.c_str(), WATERIUS_HOST_LEN);
        //LOG_INFO(FPSTR(S_CFG), "default waterius_host=" << waterius_host);
        log_cfg.info(F("default waterius_host=%s"), waterius_host.c_str());
#endif

#ifdef WATERIUS_EMAIL
        #pragma message(VAR_NAME_VALUE(WATERIUS_EMAIL))
        strncpy0(sett.waterius_email, VALUE(WATERIUS_EMAIL), EMAIL_LEN);
        //LOG_INFO(FPSTR(S_CFG), F("default waterius email=") << VALUE(WATERIUS_EMAIL));
        log_cfg.info(F("default waterius email=%s"), VALUE(WATERIUS_EMAIL).c_str());
#endif

#ifdef WATERIUS_KEY
        #pragma message(VAR_NAME_VALUE(WATERIUS_KEY))
        strncpy0(sett.waterius_key, VALUE(WATERIUS_KEY), WATERIUS_KEY_LEN);
        LOG_INFO(FPSTR(S_CFG), F("default waterius key=") << VALUE(WATERIUS_KEY));
        log_cfg.info(F("default waterius key=%s"), VALUE(WATERIUS_KEY).c_str());
#else
        //LOG_INFO(FPSTR(S_CFG), F("Generate waterius key"));
        log_cfg.info(F("Generate waterius key"));
        WateriusHttps::generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, 
                                           sett.waterius_email);
#endif

#if defined(SSID_NAME) 
#if defined(SSID_PASS)
        #pragma message(VAR_NAME_VALUE(SSID_NAME))
        #pragma message(VAR_NAME_VALUE(SSID_PASS))
        WiFi.begin(VALUE(SSID_NAME), VALUE(SSID_PASS), 0, NULL, false);  //connect=false, т.к. мы следом вызываем Wifi.begin
        //LOG_INFO(FPSTR(S_CFG), F("default ssid=") << VALUE(SSID_NAME) << F(", pwd=") << VALUE(SSID_PASS));
        log_cfg.info(F("default ssid=%s, pwd=%s"), VALUE(SSID_NAME).c_str(), VALUE(SSID_PASS).c_str());
        
        sett.crc = FAKE_CRC; //чтобы больше не попадать сюда
        return true;
#endif
#endif
        return false;
    }
}
         