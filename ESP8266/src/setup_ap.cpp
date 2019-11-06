
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


#define IMPULS_LIMIT_1 3  // Если пришло импульсов меньше 3, то перед нами 10л/имп. Если больше, то 1л/имп.

uint8_t get_factor() {
    return (runtime_data.impulses1 - data.impulses1 <= IMPULS_LIMIT_1) ? 10 : 1;
}

void update_data(String &message)
{
    if (masterI2C.getSlaveData(runtime_data)) {
        String state0good(FPSTR(S_STATE_NULL));
        String state0bad(FPSTR(S_STATE_BAD));
        String state1good(FPSTR(S_STATE_NULL));
        String state1bad(FPSTR(S_STATE_BAD));
        uint32_t delta0 = runtime_data.impulses0 - data.impulses0;
        uint32_t delta1 = runtime_data.impulses1 - data.impulses1;
        
        if (delta0 > 0) {
            state0good = FPSTR(S_STATE_CONNECTED);
            state0bad = FPSTR(S_STATE_NULL);
        }
        if (delta1 > 0) {
            state1good = FPSTR(S_STATE_CONNECTED);
            state1bad = FPSTR(S_STATE_NULL);
        }

        message = "{\"state0good\": ";
        message += state0good;
        message += ", \"state0bad\": ";
        message += state0bad;
        message += ", \"state1good\": ";
        message += state1good;
        message += ", \"state1bad\": ";
        message += state1bad;
        message += ", \"factor\": ";
        message += String(get_factor());
        message += " }";
    }
    else {
        message = "{\"error\": \"Ошибка\"}";
    }
}

WiFiManager wm;

void handleStates(){
  String message;
  message.reserve(200);
  update_data(message);
  wm.server->send(200, FPSTR(HTTP_TEXT_PLAIN), message);
}

void bindServerCallback(){
  wm.server->on(FPSTR(S_STATES), handleStates);
}


void setup_ap(Settings &sett, const SlaveData &data, const CalculatedData &cdata) 
{
    wm.setClass("dummy1");
    wm.debugPlatformInfo();
    wm.setWebServerCallback(bindServerCallback);

    LOG_NOTICE(FPSTR(S_AP), FPSTR(S_CAPTIVE_PORTAL));
    
    // Настройки HTTP 

    WiFiManagerParameter param_waterius_email( "wmail", "Электронная почта с сайта waterius.ru",  sett.waterius_email, EMAIL_LEN-1);
    wm.addParameter( &param_waterius_email);

    // Чекбокс доп. настроек

    WiFiManagerParameter checkbox("<br><br><label class='cnt'>Доп. настройки<input type='checkbox' id='chbox' name='chbox' onclick='showMe()'><span class='mrk'></span></label>");
    wm.addParameter(&checkbox);

    WiFiManagerParameter div_start("<div id='advanced' style='display:none'>");
    wm.addParameter(&div_start);
    
    // Сервер http запроса 

    WiFiManagerParameter param_waterius_host( "whost", "Адрес сервера (включает отправку)",  sett.waterius_host, WATERIUS_HOST_LEN-1);
    wm.addParameter( &param_waterius_host );


    // Настройки Blynk.сс

    WiFiManagerParameter label_blynk("<h3>Blynk.cc</h3>");
    wm.addParameter( &label_blynk);
    WiFiManagerParameter param_blynk_host( "bhost", "Адрес сервера",  sett.blynk_host, BLYNK_HOST_LEN-1);
    wm.addParameter( &param_blynk_host );
    WiFiManagerParameter param_blynk_key( "bkey", "Уникальный ключ (включает отправку)",  sett.blynk_key, BLYNK_KEY_LEN-1);
    wm.addParameter( &param_blynk_key );
    WiFiManagerParameter param_blynk_email( "bemail", "Адрес эл. почты (включает ежедневные письма)",  sett.blynk_email, EMAIL_LEN-1);
    wm.addParameter( &param_blynk_email );
    WiFiManagerParameter param_blynk_email_title( "btitle", "Тема письма",  sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN-1);
    wm.addParameter( &param_blynk_email_title );
    WiFiManagerParameter param_blynk_email_template( "btemplate", "Текст письма",  sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN-1);
    wm.addParameter( &param_blynk_email_template );

    // Настройки MQTT
    
    WiFiManagerParameter label_mqtt("<h3>MQTT</h3>");
    wm.addParameter( &label_mqtt);
    WiFiManagerParameter param_mqtt_host( "mhost", "Адрес сервера (включает отправку)<br/>Пример: broker.hivemq.com",  sett.mqtt_host, MQTT_HOST_LEN-1);
    wm.addParameter( &param_mqtt_host );
    LongParameter param_mqtt_port( "mport", "Порт",  sett.mqtt_port);
    wm.addParameter( &param_mqtt_port );
    WiFiManagerParameter param_mqtt_login( "mlogin", "Логин",  sett.mqtt_login, MQTT_LOGIN_LEN-1);
    wm.addParameter( &param_mqtt_login );
    WiFiManagerParameter param_mqtt_password( "mpassword", "Пароль",  sett.mqtt_password, MQTT_PASSWORD_LEN-1);
    wm.addParameter( &param_mqtt_password );
    WiFiManagerParameter param_mqtt_topic( "mtopic", "Topic",  sett.mqtt_topic, MQTT_TOPIC_LEN-1);
    wm.addParameter( &param_mqtt_topic );
    
    // конец доп. настроек
    WiFiManagerParameter div_end("</div>");
    wm.addParameter(&div_end);
    
    // Счетчиков
    WiFiManagerParameter cold_water("<h3>Холодная вода</h3>");
    wm.addParameter(&cold_water);

    WiFiManagerParameter label_cold_info("<p>Спустите унитаз 1-3 раза (или вылейте не меньше 4л), пока надпись не сменится на &laquoподключен&raquo. Если статус &laquoне подключен&raquo, проверьте провод в разъёме. Ватериус так определяет типа счётчика.</p>");
    wm.addParameter( &label_cold_info);

    WiFiManagerParameter label_cold_state("<b><p class='bad' id='state1bad'></p><p class='good' id='state1good'></p></b>");
    wm.addParameter( &label_cold_state);

    WiFiManagerParameter label_cold("<label class='cold'>Показания холодной воды (xxx.xx)</label>");
    wm.addParameter( &label_cold);
    FloatParameter param_channel1_start( "ch1", "",  cdata.channel1);
    wm.addParameter( &param_channel1_start);

    WiFiManagerParameter hot_water("<h3>Горячая вода</h3>");
    wm.addParameter(&hot_water);

    WiFiManagerParameter label_hot_info("<p>Откройте кран горячей воды, пока надпись не сменится на &laquoподключен&raquo.</p>");
    wm.addParameter( &label_hot_info);
    
    WiFiManagerParameter label_hot_state("<b><p class='bad' id='state0bad'></p><p class='good' id='state0good'></p></b>");
    wm.addParameter( &label_hot_state );

    WiFiManagerParameter label_hot("<label class='hot'>Показания горячей воды (xxx.xx)</label>");
    wm.addParameter( &label_hot);
    FloatParameter param_channel0_start( "ch0", "",  cdata.channel0);
    wm.addParameter( &param_channel0_start);

    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(ESP_CONNECT_TIMEOUT);
    
    LOG_NOTICE(FPSTR(S_AP), FPSTR(S_START_CONFIG_PORTAL));

    // Запуск веб сервера на 192.168.4.1
    wm.startConfigPortal( AP_NAME );

    // Успешно подключились к Wi-Fi, можно засыпать
    LOG_NOTICE(FPSTR(S_AP), FPSTR(S_CONNECTED_TO_WIFI));

    // Переписываем введенные пользователем значения в Конфигурацию

    strncpy0(sett.waterius_email, param_waterius_email.getValue(), EMAIL_LEN);
    strncpy0(sett.waterius_host, param_waterius_host.getValue(), WATERIUS_HOST_LEN);

    // Генерируем ключ используя и введенную эл. почту
    if (strnlen(sett.waterius_key, WATERIUS_KEY_LEN) == 0) {
        LOG_NOTICE(FPSTR(S_CFG), FPSTR(S_GENERATE_WATERIUS_KEY));
        WateriusHttps::generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, 
                                           sett.waterius_email);
    }

    strncpy0(sett.blynk_key, param_blynk_key.getValue(), BLYNK_KEY_LEN);
    strncpy0(sett.blynk_host, param_blynk_host.getValue(), BLYNK_HOST_LEN);
    strncpy0(sett.blynk_email, param_blynk_email.getValue(), EMAIL_LEN);
    strncpy0(sett.blynk_email_title, param_blynk_email_title.getValue(), BLYNK_EMAIL_TITLE_LEN);
    strncpy0(sett.blynk_email_template, param_blynk_email_template.getValue(), BLYNK_EMAIL_TEMPLATE_LEN);

    strncpy0(sett.mqtt_host, param_mqtt_host.getValue(), MQTT_HOST_LEN);
    strncpy0(sett.mqtt_login, param_mqtt_login.getValue(), MQTT_LOGIN_LEN);
    strncpy0(sett.mqtt_password, param_mqtt_password.getValue(), MQTT_PASSWORD_LEN);
    strncpy0(sett.mqtt_topic, param_mqtt_topic.getValue(), MQTT_TOPIC_LEN);
    sett.mqtt_port = param_mqtt_port.getValue();    

    // Текущие показания счетчиков
    sett.channel0_start = param_channel0_start.getValue();
    sett.channel1_start = param_channel1_start.getValue();

    sett.liters_per_impuls = get_factor(); //param_litres_per_imp.getValue();
    LOG_NOTICE(FPSTR(S_AP), "factor=" << sett.liters_per_impuls );

    // Запоминаем кол-во импульсов Attiny соответствующих текущим показаниям счетчиков
    sett.impulses0_start = data.impulses0;
    sett.impulses1_start = data.impulses1;

    // Предыдущие показания счетчиков. Вносим текущие значения.
    sett.impulses0_previous = sett.impulses0_start;
    sett.impulses1_previous = sett.impulses1_start;

    LOG_NOTICE(FPSTR(S_AP), "impulses0=" << sett.impulses0_start );
    LOG_NOTICE(FPSTR(S_AP), "impulses1=" << sett.impulses1_start );

    sett.crc = FAKE_CRC; // todo: сделать нормальный crc16
    storeConfig(sett);
}
