
#include "wifi_settings.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
#include <ESP8266WiFi.h>
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"

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
        LOG_ERROR("CFG", "Config stored FAILED");
    }
    else
    {
        LOG_NOTICE("CFG", "Config stored OK");
    }
}


/* Загружаем конфигурацию в EEPROM. true - успех. */
bool loadConfig(struct Settings &sett) 
{
    EEPROM.begin(sizeof(sett));  //  4 до 4096 байт. с адреса 0x7b000.
    EEPROM.get(0, sett);

    if (sett.crc == FAKE_CRC)  // todo: сделать нормальный crc16
    {
        LOG_NOTICE("CFG", "CRC ok");

        // Для безопасной работы с буферами,  в библиотеках может не быть проверок
        sett.blynk_key[BLYNK_KEY_LEN-1] = '\0';
        sett.blynk_host[BLYNK_HOST_LEN-1] = '\0';
        sett.email[EMAIL_LEN-1] = '\0';
        sett.email_title[EMAIL_TITLE_LEN-1] = '\0';
        sett.email_template[EMAIL_TEMPLATE_LEN-1] = '\0'; 
        sett.waterius_host[WATERIUS_HOST_LEN-1] = '\0';
        sett.waterius_key[WATERIUS_KEY_LEN-1] = '\0';
        
        LOG_NOTICE( "CFG", " email=" << sett.email);
        LOG_NOTICE( "CFG", " waterius host=" << sett.waterius_host << " key=" << sett.waterius_key);

        //TODO
        //if (strlen(sett.waterius_key) == 0) { генерируем новый }

        // Всегда одно и тоже будет
        LOG_NOTICE( "CFG", "channel0_start=" << sett.channel0_start << ", impules0_start=" << sett.impules0_start << ", factor=" << sett.liters_per_impuls );
        LOG_NOTICE( "CFG", "channel1_start=" << sett.channel1_start << ", impules1_start=" << sett.impules1_start);
        
        return true;

    } else {    
        // Конфигурация не была сохранена в EEPROM, инициализируем с нуля
        LOG_NOTICE( "CFG", "crc failed=" << sett.crc );

        // Заполняем нулями всю конфигурацию
        memset(&sett, 0, sizeof(sett));

        sett.version = CURRENT_VERSION;  //для совместимости в будущем

        sett.liters_per_impuls = LITRES_PER_IMPULS_DEFAULT;
        
        strncpy0(sett.blynk_host, BLYNK_DEFAULT_DOMAIN, BLYNK_HOST_LEN);

        sett.email[0] = '\0';

        String email_title = "Новые показания {DEVICE_NAME}";
        strncpy0(sett.email_title, email_title.c_str(), EMAIL_TITLE_LEN);

        String email_template = "Горячая: {V0}м3, Холодная: {V1}м3<br>За день:<br>Горячая: +{V3}л, Холодная: +{V4}л<br>Напряжение:{V2}В";
        strncpy0(sett.email_template, email_template.c_str(), EMAIL_TEMPLATE_LEN);

        strncpy0(sett.waterius_host, WATERIUS_DEFAULT_DOMAIN, WATERIUS_HOST_LEN);

        LOG_NOTICE("CFG", "version=" << sett.version);


//Можно задать константы при компиляции, чтобы Вотериус сразу заработал

#ifdef BLYNK_KEY    
        #pragma message(VAR_NAME_VALUE(BLYNK_KEY))
        String key = VALUE(BLYNK_KEY);
        strncpy0(sett.blynk_key, key.c_str(), BLYNK_KEY_LEN);
        LOG_NOTICE("CFG", "default key=" << key);
#endif

#ifdef WATERIUS_HOST
        #pragma message(VAR_NAME_VALUE(WATERIUS_HOST))
        String waterius_host = VALUE(WATERIUS_HOST);
        strncpy0(sett.waterius_host, waterius_host.c_str(), WATERIUS_HOST_LEN);
        LOG_NOTICE("CFG", "default waterius_host=" << waterius_host);
#endif

#ifdef SSID_NAME && SSID_PASS
        #pragma message(VAR_NAME_VALUE(SSID_NAME))
        #pragma message(VAR_NAME_VALUE(SSID_PASS))
        WiFi.begin(VALUE(SSID_NAME), VALUE(SSID_PASS), 0, NULL, false);  //connect=false, т.к. мы следом вызываем Wifi.begin
        LOG_NOTICE("CFG", "default ssid=" << VALUE(SSID_NAME) << ", pwd=" << VALUE(SSID_PASS));
        
        sett.crc = FAKE_CRC; //чтобы больше не попадать сюда
        return true;
#endif
        return false;
    }
}
         