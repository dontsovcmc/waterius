
#include "wifi_settings.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
#include <ESP8266WiFi.h>
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"

//Конвертируем значение переменных компиляции в строки
#define DEF2STR2(x) #x
#define DEF2STR(x) DEF2STR2(x)

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
        sett.key[KEY_LEN-1] = '\0';
        sett.hostname[HOSTNAME_LEN-1] = '\0';
        sett.email[EMAIL_LEN-1] = '\0';
        sett.email_title[EMAIL_TITLE_LEN-1] = '\0';
        sett.email_template[EMAIL_TEMPLATE_LEN-1] = '\0'; 
        sett.hostname_json[HOSTNAME_JSON_LEN-1] = '\0';
        
        LOG_NOTICE( "CFG", " email=" << sett.email  << ", hostname=" << sett.hostname);
        LOG_NOTICE( "CFG", " hostname_json=" << sett.hostname_json);

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
        
        String hostname = BLYNK_DEFAULT_DOMAIN;
        strncpy0(sett.hostname, hostname.c_str(), HOSTNAME_LEN);

        sett.email[EMAIL_LEN-1] = '\0';

        String email_title = "Новые показания {DEVICE_NAME}";
        strncpy0(sett.email_title, email_title.c_str(), EMAIL_TITLE_LEN);

        String email_template = "Горячая: {V0}м3, Холодная: {V1}м3<br>За день:<br>Горячая: +{V3}л, Холодная: +{V4}л<br>Напряжение:{V2}В";
        strncpy0(sett.email_template, email_template.c_str(), EMAIL_TEMPLATE_LEN);

        sett.hostname_json[0] = '\0';

        LOG_NOTICE("CFG", "version=" << sett.version << ", hostname=" << hostname);


//Можно задать константы при компиляции, чтобы Вотериус сразу заработал
#ifdef BLYNK_KEY
        String key = DEF2STR(BLYNK_KEY);
        strncpy0(sett.key, key.c_str(), KEY_LEN);
        LOG_NOTICE("CFG", "default key=" << key);
#endif
#ifdef WATERIUS_EMAIL
        String email = DEF2STR(WATERIUS_EMAIL);
        strncpy0(sett.email, email.c_str(), EMAIL_LEN);
        LOG_NOTICE("CFG", "default email=" << email);
#endif

#ifdef HOSTNAME_JSON
        String hostname_json = DEF2STR(HOSTNAME_JSON);
        strncpy0(sett.hostname_json, hostname_json.c_str(), HOSTNAME_JSON_LEN);
        LOG_NOTICE("CFG", "default hostname_json=" << hostname_json);
#endif

#ifdef SSID_NAME && SSID_PASS
        WiFi.begin(DEF2STR(SSID_NAME), DEF2STR(SSID_PASS));
        LOG_NOTICE("CFG", "default ssid=" << DEF2STR(SSID_NAME) << ", pwd=" << DEF2STR(SSID_PASS));
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < ESP_CONNECT_TIMEOUT) {
            LOG_NOTICE("WIF", "Wifi default status: " << WiFi.status());
            delay(200);
        }
        return WiFi.status() == WL_CONNECTED;
#endif
        return false;
    }
}
         