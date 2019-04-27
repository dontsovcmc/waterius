
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
#include "master_i2c.h"

#define AP_NAME "Waterius_" FIRMWARE_VERSION

extern SlaveData data;
extern MasterI2C masterI2C;

SlaveData runtime_data;

const char STATE_START[] PROGMEM = "\"Откройте воду, проверим соединение\"";
const char STATE_CONNECTED[] PROGMEM = "\"Подключен\"";


void update_data(String &message)
{
    if (masterI2C.getSlaveData(runtime_data)) {
        String state0(STATE_START);
        String state1(STATE_START);
        uint32_t delta0 = runtime_data.impulses0 - data.impulses0;
        uint32_t delta1 = runtime_data.impulses1 - data.impulses1;
        
        if (delta0 > 0) {
            state0 = STATE_CONNECTED;
        }
        if (delta1 > 0) {
            state1 = STATE_CONNECTED;
        }

        message = "{\"state0\": " + state0
                + ", \"state1\": " + state1 
                + ", \"delta0\": " + delta0
                + ", \"delta1\": " + delta1
                + " }";
    }
    else {
        message = "{\"error\": \"connect failed\"}";
    }
}

const char LABEL_WATERIUS[] PROGMEM       = "<label><b>Waterius.ru</b></br></label>";
const char LABEL_WATERIUS_EMAIL[] PROGMEM       = "<label>Адрес эл. почты на waterius.ru</label>";

const char LABEL_BLYNK[] PROGMEM       = "<label><b>Blynk.cc</b></br></label>";

const char LABEL_MQTT[] PROGMEM       = "<label><b>MQTT</b></br></label>";

const char LABEL_COUNTERS[] PROGMEM       = "<label><b>Счётчики</b></br></label>";

const char LABEL_GET_IMPULSES[] PROGMEM         = "Получено: <a id=\"delta0\"></a> имп. и <a id=\"delta1\"></a> имп.</br>";
const char LABEL_LITRES_PER_IMP[] PROGMEM       = "<label>Литров на импульс</label>";
const char LABEL_CHANNEL0[] PROGMEM             = "<label><b>Вход 0</b>: </label><label id=\"state0\"></label></br>";
const char LABEL_CHANNEL1[] PROGMEM             = "<label><b>Вход 1</b>: </label><label id=\"state1\"></label></br>";
const char LABEL_CHANNELS[] PROGMEM       = "<label>После подключения, введите текущее значение счётчиков в кубометрах</label></br>";
const char LABEL_CHANNEL0_VALUE[] PROGMEM       = "<label>Вход 0:</label>";
const char LABEL_CHANNEL1_VALUE[] PROGMEM       = "<label>Вход 1:</label>";
const char WATERIUS_CALLBACK[] PROGMEM = "<script>\
    let timerId = setTimeout(function run() {\
        const xhr = new XMLHttpRequest();\
        xhr.open('GET', '/states');\
        xhr.send();\
        xhr.onreadystatechange = function (e) {\
            if(xhr.readyState === 4 && xhr.status === 200) {\
                var data = JSON.parse(xhr.responseText);\
                Object.keys(data).forEach(function(key) {\
                    document.getElementById(key).innerHTML = data[key];\
                })\
            };\
        };\
        timerId = setTimeout(run, 1000);\
    }, 1000);\
</script>";

void setup_ap(Settings &sett, const SlaveData &data, const float &channel0, const float &channel1) 
{
    LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
    
    WiFiManager wm;

    wm.setWateriusCallback(&update_data);

    LOG_NOTICE( "AP", "User requested captive portal" );
    
    // Настройки Waterius.ru
    WiFiManagerParameter label_waterius(LABEL_WATERIUS);
    WiFiManagerParameter label_waterius_email(LABEL_WATERIUS_EMAIL);
    WiFiManagerParameter param_waterius_email( "wmail", "Адрес эл. почты:",  sett.waterius_email, EMAIL_LEN-1);
    WiFiManagerParameter param_waterius_host( "whost", "Waterius сервер",  sett.waterius_host, WATERIUS_HOST_LEN-1);
    WiFiManagerParameter param_waterius_key( "wkey", "Waterius ключ",  sett.waterius_key, WATERIUS_KEY_LEN-1);

    // Настройки Blynk.сс
    WiFiManagerParameter label_blynk(LABEL_BLYNK);
    WiFiManagerParameter param_blynk_host( "bhost", "Blynk сервер",  sett.blynk_host, BLYNK_HOST_LEN-1);
    WiFiManagerParameter param_blynk_key( "bkey", "Blynk ключ",  sett.blynk_key, BLYNK_KEY_LEN-1);

    WiFiManagerParameter param_blynk_email( "bemail", "Адрес эл. почты",  sett.blynk_email, EMAIL_LEN-1);
    WiFiManagerParameter param_blynk_email_title( "btitle", "Заголовок",  sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN-1);
    WiFiManagerParameter param_blynk_email_template( "btemplate", "Тело письма",  sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN-1);

    // Настройки MQTT
    WiFiManagerParameter label_mqtt(LABEL_MQTT);
    WiFiManagerParameter param_mqtt_host( "mhost", "MQTT сервер",  sett.mqtt_host, MQTT_HOST_LEN-1);
    LongParameter param_mqtt_port( "mport", "Blynk ключ",  sett.mqtt_port);
    WiFiManagerParameter param_mqtt_login( "mlogin", "MQTT логин",  sett.mqtt_login, MQTT_LOGIN_LEN-1);
    WiFiManagerParameter param_mqtt_password( "mpassword", "MQTT пароль",  sett.mqtt_password, MQTT_LOGIN_LEN-1);
    WiFiManagerParameter param_mqtt_topic_c0( "mtopic_c0", "MQTT topic для входа 0",  sett.mqtt_topic_c0, MQTT_TOPIC_C0_LEN-1);
    WiFiManagerParameter param_mqtt_topic_c1( "mtopic_c1", "MQTT topic для входа 1",  sett.mqtt_topic_c1, MQTT_TOPIC_C1_LEN-1);
    WiFiManagerParameter param_mqtt_topic_bat( "mtopic_bat", "MQTT topic для напряжения батареи",  sett.mqtt_topic_bat, MQTT_TOPIC_BAT_LEN-1);
    
    // Счетчиков
    WiFiManagerParameter label_counters(LABEL_COUNTERS);
    WiFiManagerParameter label_get_impulses(LABEL_GET_IMPULSES);
    WiFiManagerParameter label_litres_per_imp(LABEL_LITRES_PER_IMP);
    LongParameter param_litres_per_imp( "factor", "",  sett.liters_per_impuls);
    WiFiManagerParameter label_channel0(LABEL_CHANNEL0);
    WiFiManagerParameter label_channel1(LABEL_CHANNEL1);

    WiFiManagerParameter label_channels(LABEL_CHANNELS);
    WiFiManagerParameter label_channel0_value(LABEL_CHANNEL0_VALUE);
    FloatParameter param_channel0_start( "ch0", "xxx.xx",  channel0);
    WiFiManagerParameter label_channel1_value(LABEL_CHANNEL1_VALUE);
    FloatParameter param_channel1_start( "ch1", "xxx.xx",  channel1);

    WiFiManagerParameter javascript_callback(WATERIUS_CALLBACK);

    wm.addParameter( &label_waterius);
    wm.addParameter( &label_waterius_email);
    wm.addParameter( &param_waterius_email);
    
#ifndef ONLY_CLOUD_WATERIUS    
    wm.addParameter( &param_waterius_host );
    wm.addParameter( &param_waterius_key );

    wm.addParameter( &label_blynk);
    wm.addParameter( &param_blynk_host );
    wm.addParameter( &param_blynk_key );
    wm.addParameter( &param_blynk_email );
    wm.addParameter( &param_blynk_email_title );
    wm.addParameter( &param_blynk_email_template );
#endif

    wm.addParameter( &label_mqtt);
    wm.addParameter( &param_mqtt_host );
    wm.addParameter( &param_mqtt_port );
    wm.addParameter( &param_mqtt_login );
    wm.addParameter( &param_mqtt_password );
    wm.addParameter( &param_mqtt_topic_c0 );
    wm.addParameter( &param_mqtt_topic_c1 );
    wm.addParameter( &param_mqtt_topic_bat );

    wm.addParameter( &label_counters);
    wm.addParameter( &label_get_impulses);
    wm.addParameter( &label_litres_per_imp);
    wm.addParameter( &param_litres_per_imp);

    wm.addParameter( &label_channel0);
    wm.addParameter( &label_channel1);

    wm.addParameter( &label_channels);
    wm.addParameter( &label_channel0_value);
    wm.addParameter( &param_channel0_start );
    wm.addParameter( &label_channel1_value);
    wm.addParameter( &param_channel1_start );;

    wm.addParameter( &javascript_callback);

    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(ESP_CONNECT_TIMEOUT);
    
    LOG_NOTICE( "AP", "start config portal" );

    // Запуск веб сервера на 192.168.4.1
    wm.startConfigPortal( AP_NAME );

    // Успешно подключились к Wi-Fi, можно засыпать
    LOG_NOTICE( "AP", "Connected to wifi. Save settings, go to sleep" );

    // Переписываем введенные пользователем значения в Конфигурацию

    // JSON
    strncpy0(sett.waterius_email, param_waterius_email.getValue(), EMAIL_LEN);

#ifndef ONLY_CLOUD_WATERIUS      
    strncpy0(sett.waterius_key, param_waterius_key.getValue(), WATERIUS_KEY_LEN);
    strncpy0(sett.waterius_host, param_waterius_host.getValue(), WATERIUS_HOST_LEN);
    // Blynk
    strncpy0(sett.blynk_key, param_blynk_key.getValue(), BLYNK_KEY_LEN);
    strncpy0(sett.blynk_host, param_blynk_host.getValue(), BLYNK_HOST_LEN);
    strncpy0(sett.blynk_email, param_blynk_email.getValue(), EMAIL_LEN);
    strncpy0(sett.blynk_email_title, param_blynk_email_title.getValue(), BLYNK_EMAIL_TITLE_LEN);
    strncpy0(sett.blynk_email_template, param_blynk_email_template.getValue(), BLYNK_EMAIL_TEMPLATE_LEN);
#endif

    // MQTT
    strncpy0(sett.mqtt_host, param_mqtt_host.getValue(), MQTT_HOST_LEN);
    strncpy0(sett.mqtt_login, param_mqtt_login.getValue(), MQTT_LOGIN_LEN);
    strncpy0(sett.mqtt_password, param_mqtt_password.getValue(), MQTT_PASSWORD_LEN);
    strncpy0(sett.mqtt_topic_c0, param_mqtt_topic_c0.getValue(), MQTT_TOPIC_C0_LEN);
    strncpy0(sett.mqtt_topic_c1, param_mqtt_topic_c1.getValue(), MQTT_TOPIC_C1_LEN);
    strncpy0(sett.mqtt_topic_bat, param_mqtt_topic_bat.getValue(), MQTT_TOPIC_BAT_LEN);
    sett.mqtt_port = param_mqtt_port.getValue();
    
    if (strnlen(sett.waterius_key, WATERIUS_KEY_LEN) == 0) {
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
