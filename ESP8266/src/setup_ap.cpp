
#include "setup_ap.h"
#include "Logging.h"
#include "wifi_settings.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiClient.h>
#include <EEPROM.h>
#include "utils.h"

#define AP_NAME "Waterius_0.4.5"

void setup_ap(Settings &sett, const SlaveData &data, const float &channel0, const float &channel1) 
{
    LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
    
    //WiFi.mode(WIFI_AP);
    
    WiFiManager wm;
    LOG_NOTICE( "AP", "User requested captive portal" );
    
    WiFiManagerParameter param_key( "key", "key",  sett.key, KEY_LEN-1);
    wm.addParameter( &param_key );

    WiFiManagerParameter param_hostname( "host", "host",  sett.hostname, HOSTNAME_LEN-1);
    wm.addParameter( &param_hostname );

    WiFiManagerParameter param_email( "email", "email",  sett.email, EMAIL_LEN-1);
    wm.addParameter( &param_email );
    
    WiFiManagerParameter param_email_title( "title", "title",  sett.email_title, EMAIL_TITLE_LEN-1);
    wm.addParameter( &param_email_title );
    
    WiFiManagerParameter param_email_template( "template", "template",  sett.email_template, EMAIL_TEMPLATE_LEN-1);
    wm.addParameter( &param_email_template );

    FloatParameter param_channel0_start( "channel0", "channel0",  channel0);
    wm.addParameter( &param_channel0_start );
    
    FloatParameter param_channel1_start( "channel1", "channel1",  channel1);
    wm.addParameter( &param_channel1_start );

    LongParameter param_litres_per_imp( "factor", "factor",  sett.liters_per_impuls);
    wm.addParameter( &param_litres_per_imp );

    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(15);
    
    LOG_NOTICE( "AP", "start config portal" );

    // Запуск веб сервера на 192.168.4.1
    wm.startConfigPortal( AP_NAME );

    // Успешно подключились к Wi-Fi, можно засыпать
    LOG_NOTICE( "AP", "Connected to wifi. Save settings, go to sleep" );

    // Переписываем введенные пользователем значения в Конфигурацию
    strncpy0(sett.key, param_key.getValue(), KEY_LEN);
    strncpy0(sett.hostname, param_hostname.getValue(), HOSTNAME_LEN);
    strncpy0(sett.email, param_email.getValue(), EMAIL_LEN);
    strncpy0(sett.email_title, param_email_title.getValue(), EMAIL_TITLE_LEN);
    strncpy0(sett.email_template, param_email_template.getValue(), EMAIL_TEMPLATE_LEN);

    // Текущие показания счетчиков
    sett.channel0_start = param_channel0_start.getValue();
    sett.channel1_start = param_channel1_start.getValue();
    
    // Предыдущие показания счетчиков. Вносим текущие значения.
    sett.channel0_previous = param_channel0_start.getValue();
    sett.channel1_previous = param_channel1_start.getValue();

    sett.liters_per_impuls = param_litres_per_imp.getValue();

    // Запоминаем кол-во импульсов Attiny соответствующих текущим показаниям счетчиков
    sett.impules0_start = data.impulses0;
    sett.impules1_start = data.impulses1;

    LOG_NOTICE( "AP", "impulses0=" << sett.impules0_start );
    LOG_NOTICE( "AP", "impulses1=" << sett.impules1_start );

    sett.crc = FAKE_CRC; // todo: сделать нормальный crc16
    storeConfig(sett);
}
