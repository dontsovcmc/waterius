
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

const char STATE_START[] PROGMEM = "\"не подключен\"";
const char STATE_CONNECTED[] PROGMEM = "\"подключен\"";

#define IMPULS_LIMIT_1 3  // Если пришло импульсов меньше 3, то перед нами 10л/имп. Если больше, то 1л/имп.

uint8_t get_factor() {
    return (runtime_data.impulses1 - data.impulses1 <= IMPULS_LIMIT_1) ? 10 : 1;
}

void update_data(String &message)
{
    if (masterI2C.getSlaveData(runtime_data)) {
        String state0(STATE_START);
        String state1(STATE_START);
        String factor;
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
                + ", \"factor\": " + String(get_factor())
                + " }";
    }
    else {
        message = "{\"error\": \"connect failed\"}";
    }
}

const char LABEL_WATERIUS_EMAIL[] PROGMEM       = "<label>Введите электронную почту, которую используете на сайте waterius.ru</label>";

const char LABEL_BLYNK[] PROGMEM       = "<label><b>Blynk.cc</b></br></label>";

const char LABEL_MQTT[] PROGMEM       = "<label><b>MQTT</b></br></label>";

const char LABEL_COLD[] PROGMEM       = "<label><b>Счётчик холодной воды</b></br></label>";
const char LABEL_COLD_INFO[] PROGMEM       = "<label>Спустите унитаз 1-3 раза, пока надпись не сменится на &quotподключен&quot. Если статус &quotне подключен&quot, проверьте подключение провода.</br></label>";
const char LABEL_COLD_STATE[] PROGMEM             = "<label class=\"cold\" id=\"state0\"></label></br>";

const char LABEL_VALUE[] PROGMEM             = "<label>Введите показания:</label></br>";

const char LABEL_HOT[] PROGMEM       = "<label><b>Счётчик горячей воды</b></label></br>";
const char LABEL_HOT_INFO[] PROGMEM       = "<label>Откройте кран горячей воды, пока надпись не сменится на &quotподключен&quot.</label>";
const char LABEL_HOT_STATE[] PROGMEM       = "<label class=\"hot\" id=\"state1\"></label></br>";

const char LABEL_FACTOR[] PROGMEM       = "<label>Множитель: <a id=\"factor\"></a> л. на импульс</label></br>";

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

void setup_ap(Settings &sett, const SlaveData &data, const CalculatedData &cdata) 
{
    LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
    
    WiFiManager wm;

    wm.setWateriusCallback(&update_data);

    LOG_NOTICE( "AP", "User requested captive portal" );
    
#ifdef SEND_WATERIUS  // Настройки JSON 
    WiFiManagerParameter label_waterius_email(LABEL_WATERIUS_EMAIL);
    WiFiManagerParameter param_waterius_email( "wmail", "Адрес эл. почты:",  sett.waterius_email, EMAIL_LEN-1);

    wm.addParameter( &label_waterius_email);
    wm.addParameter( &param_waterius_email);

#ifndef ONLY_CLOUD_WATERIUS 
    WiFiManagerParameter param_waterius_host( "whost", "Waterius сервер",  sett.waterius_host, WATERIUS_HOST_LEN-1);
    WiFiManagerParameter param_waterius_key( "wkey", "Waterius ключ",  sett.waterius_key, WATERIUS_KEY_LEN-1);

    wm.addParameter( &param_waterius_host );
    wm.addParameter( &param_waterius_key );
#endif
#endif

#ifdef SEND_BLYNK  // Настройки Blynk.сс
    WiFiManagerParameter label_blynk(LABEL_BLYNK);
    WiFiManagerParameter param_blynk_host( "bhost", "Blynk сервер",  sett.blynk_host, BLYNK_HOST_LEN-1);
    WiFiManagerParameter param_blynk_key( "bkey", "Blynk ключ",  sett.blynk_key, BLYNK_KEY_LEN-1);

    WiFiManagerParameter param_blynk_email( "bemail", "Адрес эл. почты",  sett.blynk_email, EMAIL_LEN-1);
    WiFiManagerParameter param_blynk_email_title( "btitle", "Заголовок",  sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN-1);
    WiFiManagerParameter param_blynk_email_template( "btemplate", "Тело письма",  sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN-1);

    wm.addParameter( &label_blynk);
    wm.addParameter( &param_blynk_host );
    wm.addParameter( &param_blynk_key );
    wm.addParameter( &param_blynk_email );
    wm.addParameter( &param_blynk_email_title );
    wm.addParameter( &param_blynk_email_template );
#endif

#ifdef SEND_MQTT  // Настройки MQTT
    WiFiManagerParameter label_mqtt(LABEL_MQTT);
    WiFiManagerParameter param_mqtt_host( "mhost", "MQTT сервер",  sett.mqtt_host, MQTT_HOST_LEN-1);
    LongParameter param_mqtt_port( "mport", "MQTT порт",  sett.mqtt_port);
    WiFiManagerParameter param_mqtt_login( "mlogin", "MQTT логин",  sett.mqtt_login, MQTT_LOGIN_LEN-1);
    WiFiManagerParameter param_mqtt_password( "mpassword", "MQTT пароль",  sett.mqtt_password, MQTT_PASSWORD_LEN-1);
    WiFiManagerParameter param_mqtt_topic( "mtopic", "MQTT topic",  sett.mqtt_topic, MQTT_TOPIC_LEN-1);
    
    wm.addParameter( &label_mqtt);
    wm.addParameter( &param_mqtt_host );
    wm.addParameter( &param_mqtt_port );
    wm.addParameter( &param_mqtt_login );
    wm.addParameter( &param_mqtt_password );
    wm.addParameter( &param_mqtt_topic );
#endif

    // Счетчиков
    WiFiManagerParameter label_cold(LABEL_COLD);
    WiFiManagerParameter label_cold_info(LABEL_COLD_INFO);
    WiFiManagerParameter label_cold_state(LABEL_COLD_STATE);

    WiFiManagerParameter label_value0(LABEL_VALUE);
    FloatParameter param_channel0_start( "ch0", "xxx.xx",  cdata.channel0);

    WiFiManagerParameter label_hot(LABEL_HOT);
    WiFiManagerParameter label_hot_info(LABEL_HOT_INFO);
    WiFiManagerParameter label_hot_state(LABEL_HOT_STATE);

    WiFiManagerParameter label_value1(LABEL_VALUE);
    FloatParameter param_channel1_start( "ch1", "xxx.xx",  cdata.channel1);
    
    WiFiManagerParameter label_factor(LABEL_FACTOR);
    //LongParameter param_litres_per_imp( "factor", "",  sett.liters_per_impuls, 5, "type=\"number\"");
    
    WiFiManagerParameter javascript_callback(WATERIUS_CALLBACK);

    wm.addParameter( &label_cold);
    wm.addParameter( &label_cold_info);
    wm.addParameter( &label_cold_state);

    wm.addParameter( &label_value1);
    wm.addParameter( &param_channel1_start);

    wm.addParameter( &label_hot);
    wm.addParameter( &label_hot_info);
    wm.addParameter( &label_hot_state );

    wm.addParameter( &label_value0);
    wm.addParameter( &param_channel0_start);

    wm.addParameter( &label_factor);

    //wm.addParameter( &param_litres_per_imp);
    wm.addParameter( &javascript_callback);

    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(ESP_CONNECT_TIMEOUT);
    
    LOG_NOTICE( "AP", "start config portal" );

    // Запуск веб сервера на 192.168.4.1
    wm.startConfigPortal( AP_NAME );

    // Успешно подключились к Wi-Fi, можно засыпать
    LOG_NOTICE( "AP", "Connected to wifi. Save settings, go to sleep" );

    // Переписываем введенные пользователем значения в Конфигурацию

#ifdef SEND_WATERIUS // JSON
    strncpy0(sett.waterius_email, param_waterius_email.getValue(), EMAIL_LEN);

#ifndef ONLY_CLOUD_WATERIUS 
    strncpy0(sett.waterius_key, param_waterius_key.getValue(), WATERIUS_KEY_LEN);
    strncpy0(sett.waterius_host, param_waterius_host.getValue(), WATERIUS_HOST_LEN);
#endif

    // Генерируем ключ используя и введенную эл. почту
    if (strnlen(sett.waterius_key, WATERIUS_KEY_LEN) == 0) {
        LOG_NOTICE("CFG", "Generate waterius key");
        WateriusHttps::generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, 
                                           sett.waterius_email);
    }
#endif

#ifdef SEND_BLYNK
    strncpy0(sett.blynk_key, param_blynk_key.getValue(), BLYNK_KEY_LEN);
    strncpy0(sett.blynk_host, param_blynk_host.getValue(), BLYNK_HOST_LEN);
    strncpy0(sett.blynk_email, param_blynk_email.getValue(), EMAIL_LEN);
    strncpy0(sett.blynk_email_title, param_blynk_email_title.getValue(), BLYNK_EMAIL_TITLE_LEN);
    strncpy0(sett.blynk_email_template, param_blynk_email_template.getValue(), BLYNK_EMAIL_TEMPLATE_LEN);
#endif

#ifdef SEND_MQTT
    strncpy0(sett.mqtt_host, param_mqtt_host.getValue(), MQTT_HOST_LEN);
    strncpy0(sett.mqtt_login, param_mqtt_login.getValue(), MQTT_LOGIN_LEN);
    strncpy0(sett.mqtt_password, param_mqtt_password.getValue(), MQTT_PASSWORD_LEN);
    strncpy0(sett.mqtt_topic, param_mqtt_topic.getValue(), MQTT_TOPIC_LEN);
    sett.mqtt_port = param_mqtt_port.getValue();    
#endif


    // Текущие показания счетчиков
    sett.channel0_start = param_channel0_start.getValue();
    sett.channel1_start = param_channel1_start.getValue();

    sett.liters_per_impuls = get_factor(); //param_litres_per_imp.getValue();
    LOG_NOTICE( "AP", "factor=" << sett.liters_per_impuls );

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
