
#include "wifi_settings.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
#include <IPAddress.h>
#include <EEPROM.h>
#include "utils.h"

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
        sett.hostname_json[JSON_SERVER_LEN-1] = '\0';
        
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

        String email_title = "Новые показания {DEVICE_NAME}";
        strncpy0(sett.email_title, email_title.c_str(), EMAIL_TITLE_LEN);

        String email_template = "Горячая: {V0}м3, Холодная: {V1}м3<br>За день:<br>Горячая: +{V3}л, Холодная: +{V4}л<br>Напряжение:{V2}В";
        strncpy0(sett.email_template, email_template.c_str(), EMAIL_TEMPLATE_LEN);

        sett.hostname_json[0] = '\0';

        LOG_NOTICE("CFG", "version=" << sett.version << ", hostname=" << hostname);
        return false;
    }
}
         