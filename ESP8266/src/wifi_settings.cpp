
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
        #if LOGLEVEL>=0
        LOG_START(FPSTR(S_ERROR) ,FPSTR(S_CFG)); LOG(F("Config stored FAILED"));
        #endif
    }
    else
    {
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("Config stored OK"));
    	#endif
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
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("Configuration CRC ok"));
    	#endif

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

        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("--- Waterius.ru ---- "));
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("email=     ")); LOG(sett.waterius_email);
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("host=      ")); LOG(sett.waterius_host); LOG(F(" key=")); LOG(sett.waterius_key);
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("wakeup min=")); LOG(sett.wakeup_per_min);
        
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("--- Blynk.cc ---- ")); 
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("host=      ")); LOG(sett.blynk_host); LOG(F(" key=")); LOG(sett.blynk_key);
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("email=     ")); LOG(sett.blynk_email);

        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("--- MQTT ---- "));
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("host=      ")); LOG(sett.mqtt_host); LOG(F(" port=")); LOG(sett.mqtt_port);
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("login=     ")); LOG(sett.mqtt_login); LOG(F(" pass=")); LOG(sett.mqtt_password);
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("topic=     ")); LOG(sett.mqtt_topic);

        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("--- Network ---- "));
        if (sett.ip) {
            LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("DHCP turn off"));
        } else {
            LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("DHCP is on"));
        }
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("static_ip= ")); LOG(IPAddress(sett.ip).toString());
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("gateway=   ")); LOG(IPAddress(sett.gateway).toString());
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("mask=      ")); LOG(IPAddress(sett.mask).toString());

        // Всегда одно и тоже будет    
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("--- Counters ---- "));
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("channel0 start=")); LOG(sett.channel0_start); LOG(F(", impulses=")); LOG(sett.impulses0_start); LOG(F(", factor=")); LOG(sett.factor0);
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("channel1 start=")); LOG(sett.channel1_start); LOG(F(", impulses=")); LOG(sett.impulses1_start); LOG(F(", factor=")); LOG(sett.factor1);
    	#endif
        return true;

    } else {    
        // Конфигурация не была сохранена в EEPROM, инициализируем с нуля
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("Configuration CRC failed=")); LOG(sett.crc);
    	#endif
        
        // Заполняем нулями всю конфигурацию
        memset(&sett, 0, sizeof(sett));

        sett.version = CURRENT_VERSION;  //для совместимости в будущем
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("version=")); LOG(sett.version);
    	#endif

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
        
//Можно задать константы при компиляции, чтобы Ватериус сразу заработал

#ifdef BLYNK_KEY    
        #pragma message(VAR_NAME_VALUE(BLYNK_KEY))
        String key = VALUE(BLYNK_KEY);
        strncpy0(sett.blynk_key, key.c_str(), BLYNK_KEY_LEN);
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("default Blynk key=")); LOG(key);
    	#endif
#endif

#ifdef WATERIUS_HOST
        #pragma message(VAR_NAME_VALUE(WATERIUS_HOST))
        String waterius_host = VALUE(WATERIUS_HOST);
        strncpy0(sett.waterius_host, waterius_host.c_str(), WATERIUS_HOST_LEN);
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("default waterius_host=")); LOG(waterius_host);
    	#endif
#endif

#ifdef WATERIUS_EMAIL
        #pragma message(VAR_NAME_VALUE(WATERIUS_EMAIL))
        strncpy0(sett.waterius_email, VALUE(WATERIUS_EMAIL), EMAIL_LEN);
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("default waterius email=")); LOG(VALUE(WATERIUS_EMAIL))
    	#endif
#endif

#ifdef WATERIUS_KEY
        #pragma message(VAR_NAME_VALUE(WATERIUS_KEY))
        strncpy0(sett.waterius_key, VALUE(WATERIUS_KEY), WATERIUS_KEY_LEN);
        LOG_INFO(FPSTR(S_CFG), "default waterius key=" + String(VALUE(WATERIUS_KEY)));
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("default waterius key=")); LOG(VALUE(WATERIUS_KEY));
    	#endif
#else
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("Generate waterius key"));
    	#endif
        WateriusHttps::generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, 
                                           sett.waterius_email);
#endif

#if defined(SSID_NAME) 
#if defined(SSID_PASS)
        #pragma message(VAR_NAME_VALUE(SSID_NAME))
        #pragma message(VAR_NAME_VALUE(SSID_PASS))
        WiFi.begin(VALUE(SSID_NAME), VALUE(SSID_PASS), 0, NULL, false);  //connect=false, т.к. мы следом вызываем Wifi.begin
        #if LOGLEVEL>=1
    	LOG_START(FPSTR(S_INFO) ,FPSTR(S_CFG)); LOG(F("default ssid=")); LOG(VALUE(SSID_NAME)); LOG(F(", pwd=")); LOG(VALUE(SSID_PASS));
    	#endif
        
        sett.crc = FAKE_CRC; //чтобы больше не попадать сюда
        return true;
#endif
#endif
        return false;
    }
}
         