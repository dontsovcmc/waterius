
#include "setup_ap.h"
#include "Logging.h"
#include "wifi_settings.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiClient.h>
#include <EEPROM.h>
#include "utils.h"
#include "WateriusHttps.h"


#define AP_NAME "Waterius_" FIRMWARE_VERSION

void setup_ap(Settings &sett, const SlaveData &data, const float &channel0, const float &channel1) 
{
    LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
    
    // Без этих строчек корректно подключается к той же сети, которая до этого была
    // https://github.com/tzapu/WiFiManager/issues/511
    //WiFi.persistent(false);  
    //WiFi.disconnect(); 
    //WiFi.mode(WIFI_AP);         
    //WiFi.persistent(true);
    
    WiFiManager wm;
    LOG_NOTICE( "AP", "User requested captive portal" );
    
    FloatParameter param_channel0_start( "channel0", "Вход 0 (м3):",  channel0);
    FloatParameter param_channel1_start( "channel1", "Вход 1 (м3):",  channel1);
    LongParameter param_litres_per_imp( "factor", "Литров на импульс:",  sett.liters_per_impuls);

    WiFiManagerParameter param_waterius_host( "whost", "Waterius сервер:",  sett.waterius_host, WATERIUS_HOST_LEN-1);
    WiFiManagerParameter param_waterius_email( "wmail", "Адрес эл. почты:",  sett.waterius_email, EMAIL_LEN-1);
    WiFiManagerParameter param_waterius_key( "wkey", "Waterius ключ. Заполнится автоматически.",  sett.waterius_key, WATERIUS_KEY_LEN-1);
    
    // Настройки для Blynk 
    WiFiManagerParameter param_blynk_host( "bhost", "Blynk сервер:",  sett.blynk_host, BLYNK_HOST_LEN-1);
    WiFiManagerParameter param_blynk_key( "bkey", "Blynk ключ:",  sett.blynk_key, BLYNK_KEY_LEN-1);

    WiFiManagerParameter param_blynk_email( "bemail", "Адрес эл. почты:",  sett.blynk_email, EMAIL_LEN-1);
    WiFiManagerParameter param_blynk_email_title( "title", "Заголовок:",  sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN-1);
    WiFiManagerParameter param_blynk_email_template( "template", "Тело письма:",  sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN-1);

    wm.addParameter( &param_channel0_start );
    wm.addParameter( &param_channel1_start );
    wm.addParameter( &param_litres_per_imp );

    wm.addParameter( &param_waterius_host );
    wm.addParameter( &param_waterius_email );
    wm.addParameter( &param_waterius_key );

    wm.addParameter( &param_blynk_host );
    wm.addParameter( &param_blynk_key );
    wm.addParameter( &param_blynk_email );
    wm.addParameter( &param_blynk_email_title );
    wm.addParameter( &param_blynk_email_template );

    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(ESP_CONNECT_TIMEOUT);
    
    LOG_NOTICE( "AP", "start config portal" );

    // Запуск веб сервера на 192.168.4.1
    wm.startConfigPortal( AP_NAME );

    // Успешно подключились к Wi-Fi, можно засыпать
    LOG_NOTICE( "AP", "Connected to wifi. Save settings, go to sleep" );

    // Переписываем введенные пользователем значения в Конфигурацию

    // Blynk
    strncpy0(sett.blynk_key, param_blynk_key.getValue(), BLYNK_KEY_LEN);
    strncpy0(sett.blynk_host, param_blynk_host.getValue(), BLYNK_HOST_LEN);
    strncpy0(sett.blynk_email, param_blynk_email.getValue(), EMAIL_LEN);
    strncpy0(sett.blynk_email_title, param_blynk_email_title.getValue(), BLYNK_EMAIL_TITLE_LEN);
    strncpy0(sett.blynk_email_template, param_blynk_email_template.getValue(), BLYNK_EMAIL_TEMPLATE_LEN);

    // JSON
    strncpy0(sett.waterius_host, param_waterius_host.getValue(), WATERIUS_HOST_LEN);
    strncpy0(sett.waterius_email, param_waterius_email.getValue(), EMAIL_LEN);
    strncpy0(sett.waterius_key, param_waterius_key.getValue(), WATERIUS_KEY_LEN);
    
    if (!strlen(sett.waterius_key) ) {
        LOG_NOTICE("CFG", "Generate waterius key");
        WateriusHttps::generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, 
                                           sett.waterius_email);
    }
        

    // Текущие показания счетчиков
    sett.channel0_start = param_channel0_start.getValue();
    sett.channel1_start = param_channel1_start.getValue();

    sett.liters_per_impuls = param_litres_per_imp.getValue();

    // Запоминаем кол-во импульсов Attiny соответствующих текущим показаниям счетчиков
    sett.impulses0_start = data.impulses0;
    sett.impulses1_start = data.impulses1;

    // Предыдущие показания счетчиков. Вносим текущие значения.
    sett.impulses0_previous = sett.impulses0_start;
    sett.impulses1_previous = sett.impulses1_start;

    LOG_NOTICE( "AP", "impulses0=" << sett.impulses0_start );
    LOG_NOTICE( "AP", "impulses1=" << sett.impulses1_start );

    sett.crc = FAKE_CRC; // todo: сделать нормальный crc16
    storeConfig(sett);
}
