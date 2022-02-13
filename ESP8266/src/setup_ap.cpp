
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
#include "porting.h"
#include "LittleFS.h" 

extern SlaveData data;
extern MasterI2C masterI2C;

SlaveData runtime_data;


#define IMPULS_LIMIT_1  3  // Если пришло импульсов меньше 3, то перед нами 10л/имп. Если больше, то 1л/имп.

uint8_t get_auto_factor(uint32_t runtime_impulses, uint32_t impulses)
{
    return (runtime_impulses - impulses <= IMPULS_LIMIT_1) ? 10 : 1;
}

uint8_t get_factor(uint8_t combobox_factor, uint32_t runtime_impulses, uint32_t impulses, uint8_t cold_factor) {

    switch (combobox_factor) {
        case AUTO_IMPULSE_FACTOR: return get_auto_factor(runtime_impulses, impulses); 
        case AS_COLD_CHANNEL: return cold_factor;
        default: 
            return combobox_factor;  // 1, 10, 100
    }
}

#define SETUP_TIME_SEC 600UL //На какое время Attiny включает ESP (файл Attiny85\src\Setup.h)
void update_data(String &message)
{
    if (masterI2C.getSlaveData(runtime_data)) {
        String state0good(F("\"\""));
        String state0bad(F("\"Не подключён\""));
        String state1good(F("\"\""));
        String state1bad(F("\"Не подключён\""));

        uint32_t delta0 = runtime_data.impulses0 - data.impulses0;
        uint32_t delta1 = runtime_data.impulses1 - data.impulses1;
        
        if (delta0 > 0) {
            state0good = F("\"Подключён\"");
            state0bad = F("\"\"");
        }
        if (delta1 > 0) {
            state1good = F("\"Подключён\"");
            state1bad = F("\"\"");
        }

        message = F("{\"state0good\": ");
        message += state0good;
        message += F(", \"state0bad\": ");
        message += state0bad;
        message += F(", \"state1good\": ");
        message += state1good;
        message += F(", \"state1bad\": ");
        message += state1bad;
        message += F(", \"elapsed\": ");
        message += String((uint32_t)(SETUP_TIME_SEC - millis()/1000.0));
        message += F(", \"factor_cold_feedback\": ");
        message += String(get_auto_factor(runtime_data.impulses1, data.impulses1));
        message += F(", \"factor_hot_feedback\": ");
        message += String(get_auto_factor(runtime_data.impulses0, data.impulses0));
        message += F(", \"error\": \"\"");
        message += F("}");
    }
    else {
        message = F("{\"error\": \"Ошибка связи с МК\", \"factor_cold_feedback\": 1, \"factor_hot_feedback\": 1}");
    }
}

WiFiManager wm;
void handleStates(){
  //LOG_INFO(FPSTR(S_AP), F("/states request"));
  log_ap.debug(F("/states request"));
  String message;
  message.reserve(300); //сейчас 200
  update_data(message);
  wm.server->send(200, F("text/plain"), message);
}

void handleNetworks() {
  //LOG_INFO(FPSTR(S_AP), F("/networks request"));
  log_ap.debug(F("/networks request"));
  String message;
  message.reserve(2000);
  wm.WiFi_scanNetworks(wm.server->hasArg(F("refresh")),false); //wifiscan, force if arg refresh
  wm.getScanItemOut(message);  
  wm.server->send(200, F("text/plain"), message);
}

void handleLog(){
    String message;
    log_ap.debug(F("/networks request"));
    wm.server->send(200, F("text/plain"), message);
}

void bindServerCallback(){
  wm.server->on(F("/states"), handleStates);
  wm.server->on(F("/networks"), handleNetworks);
  wm.server->serveStatic("/log.txt", LittleFS, "/log.txt", "max-age=0");
}


void setup_ap(Settings &sett, const SlaveData &data, const CalculatedData &cdata) 
{
    wm.debugPlatformInfo();
    wm.setWebServerCallback(bindServerCallback);

    //LOG_INFO(FPSTR(S_AP), F("User requested captive portal"));
    log_ap.info(F("User requested captive portal"));
    
    // Настройки HTTP 

    WiFiManagerParameter title_email("<h3>Ваша электронная почта с сайта waterius.ru</h3><p>Заполните, чтобы увидеть показания на сайте</p>");
    wm.addParameter(&title_email);

    WiFiManagerParameter param_waterius_email("wmail", "Электронная почта",  sett.waterius_email, EMAIL_LEN-1);
    wm.addParameter(&param_waterius_email);

    // Чекбокс доп. настроек

    WiFiManagerParameter checkbox("<br><br><br><label class='cnt'>Дополнительные настройки<input type='checkbox' id='chbox' name='chbox' onclick='advSett()'><span class='mrk'></span></label>");
    wm.addParameter(&checkbox);

    WiFiManagerParameter div_start("<div id='advanced' style='display:none'>");
    wm.addParameter(&div_start);

    // Сервер http запроса 
    WiFiManagerParameter param_waterius_host( "whost", "Адрес сервера (включает отправку)",  sett.waterius_host, WATERIUS_HOST_LEN-1);
    wm.addParameter(&param_waterius_host);

    ShortParameter param_wakeup_per("mperiod", "Период отправки показаний, мин.",  sett.wakeup_per_min);
    wm.addParameter(&param_wakeup_per);

    // Настройки Blynk.сс

    WiFiManagerParameter label_blynk("<h3>Blynk.cc</h3>");
    wm.addParameter(&label_blynk);
    WiFiManagerParameter param_blynk_host( "bhost", "Адрес сервера",  sett.blynk_host, BLYNK_HOST_LEN-1);
    wm.addParameter(&param_blynk_host);
    WiFiManagerParameter param_blynk_key( "bkey", "Уникальный ключ (включает отправку)",  sett.blynk_key, BLYNK_KEY_LEN-1);
    wm.addParameter(&param_blynk_key);
    WiFiManagerParameter param_blynk_email( "bemail", "Адрес эл. почты (включает ежедневные письма)",  sett.blynk_email, EMAIL_LEN-1);
    wm.addParameter(&param_blynk_email);
    WiFiManagerParameter param_blynk_email_title( "btitle", "Тема письма",  sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN-1);
    wm.addParameter(&param_blynk_email_title);
    WiFiManagerParameter param_blynk_email_template( "btemplate", "Текст письма",  sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN-1);
    wm.addParameter(&param_blynk_email_template);

    // Настройки MQTT
    
    WiFiManagerParameter label_mqtt("<h3>MQTT</h3>");
    wm.addParameter(&label_mqtt);
    WiFiManagerParameter param_mqtt_host( "mhost", "Адрес сервера (включает отправку)<br/>Пример: broker.hivemq.com",  sett.mqtt_host, MQTT_HOST_LEN-1);
    wm.addParameter(&param_mqtt_host );

    LongParameter param_mqtt_port( "mport", "Порт",  sett.mqtt_port);
    wm.addParameter(&param_mqtt_port );
    WiFiManagerParameter param_mqtt_login( "mlogin", "Логин",  sett.mqtt_login, MQTT_LOGIN_LEN-1);
    wm.addParameter(&param_mqtt_login );
    WiFiManagerParameter param_mqtt_password( "mpassword", "Пароль",  sett.mqtt_password, MQTT_PASSWORD_LEN-1);
    wm.addParameter(&param_mqtt_password );
    WiFiManagerParameter param_mqtt_topic( "mtopic", "Topic",  sett.mqtt_topic, MQTT_TOPIC_LEN-1);
    wm.addParameter(&param_mqtt_topic );
    
    // Статический ip
    
    WiFiManagerParameter label_network("<h3>Сетевые настройки</h3>");
    wm.addParameter(&label_network);
    
    String mac("<label class=\"label\">MAC: ");
    mac += WiFi.macAddress();
    mac += "</label>";
    WiFiManagerParameter label_mac(mac.c_str());
    wm.addParameter(&label_mac);

    IPAddressParameter param_ip("ip", "Статический ip<br/>(DHCP, если равен 0.0.0.0)",  sett.ip);
    wm.addParameter(&param_ip);
    IPAddressParameter param_gw("gw", "Шлюз",  sett.gateway);
    wm.addParameter(&param_gw);
    IPAddressParameter param_mask("sn", "Маска подсети",  sett.mask);
    wm.addParameter(&param_mask);

    WiFiManagerParameter label_save_log("<h3>Лог сообщений</h3>");
    wm.addParameter(&label_save_log);

    DropdownParameter dropdown_save_log("savelog");
    dropdown_save_log.add_option(-1, "OFF", sett.SaveLogLevel);
    dropdown_save_log.add_option(0, "EMERG", sett.SaveLogLevel);
    dropdown_save_log.add_option(1, "ALERT", sett.SaveLogLevel);
    dropdown_save_log.add_option(2, "CRIT", sett.SaveLogLevel);
    dropdown_save_log.add_option(3, "ERR", sett.SaveLogLevel);
    dropdown_save_log.add_option(4, "WARNING", sett.SaveLogLevel);
    dropdown_save_log.add_option(5, "NOTICE", sett.SaveLogLevel);
    dropdown_save_log.add_option(6, "INFO", sett.SaveLogLevel);
    dropdown_save_log.add_option(7, "DEBUG", sett.SaveLogLevel);
    dropdown_save_log.add_option(8, "TRACE", sett.SaveLogLevel);
    dropdown_save_log.add_option(9, "ALL", sett.SaveLogLevel);
    wm.addParameter(&dropdown_save_log);
    
    WiFiManagerParameter label_save_log_feedback("<p id='sl_fb_control'>Уровень сообщений: <a id='save_log_feedback'></a>");
    wm.addParameter(&label_save_log_feedback);

    WiFiManagerParameter label_factor_settings("<h3>Параметры счетчиков</h3>");
    wm.addParameter(&label_factor_settings);

    WiFiManagerParameter label_cold_factor("<b>Холодная вода л/имп</b>");
    wm.addParameter(&label_cold_factor);
    
    DropdownParameter dropdown_cold_factor("factorCold");
    dropdown_cold_factor.add_option(AUTO_IMPULSE_FACTOR, "Авто", sett.factor1);
    dropdown_cold_factor.add_option(1, "1", sett.factor1);
    dropdown_cold_factor.add_option(10, "10", sett.factor1);
    dropdown_cold_factor.add_option(100, "100", sett.factor1);
    wm.addParameter(&dropdown_cold_factor);

    WiFiManagerParameter label_factor_cold_feedback("<p id='fc_fb_control'>Вес импульса: <a id='factor_cold_feedback'></a> л/имп");
    wm.addParameter(&label_factor_cold_feedback);

    WiFiManagerParameter label_hot_factor("<p><b>Горячая вода л/имп</b>");
    wm.addParameter(&label_hot_factor);
    
    DropdownParameter dropdown_hot_factor("factorHot");
    dropdown_hot_factor.add_option(AS_COLD_CHANNEL, "Как у холодной", sett.factor0);
    dropdown_hot_factor.add_option(AUTO_IMPULSE_FACTOR, "Авто", sett.factor0);
    dropdown_hot_factor.add_option(1, "1", sett.factor0);
    dropdown_hot_factor.add_option(10, "10", sett.factor0);
    dropdown_hot_factor.add_option(100, "100", sett.factor0);
    wm.addParameter(&dropdown_hot_factor);

    WiFiManagerParameter label_factor_hot_feedback("<p id='fh_fb_control'>Вес импульса: <a id='factor_hot_feedback'></a> л/имп");
    wm.addParameter( &label_factor_hot_feedback);

    // конец доп. настроек
    WiFiManagerParameter div_end("</div>");
    wm.addParameter(&div_end);
    
    // Счетчиков
    WiFiManagerParameter cold_water("<h3>Холодная вода</h3>");
    wm.addParameter(&cold_water);
            
    WiFiManagerParameter label_cold_info("<p>Спустите унитаз 1&ndash;3 раза (или вылейте не&nbsp;меньше 4&nbsp;л.), пока надпись не&nbsp;сменится на&nbsp;&laquo;подключён&raquo;. Если статус &laquo;не&nbsp;подключён&raquo;, проверьте провод в&nbsp;разъёме. Ватериус так определяет тип счётчика.</p>");
    wm.addParameter( &label_cold_info);

    WiFiManagerParameter label_cold_state("<b><p class='bad' id='state1bad'></p><p class='good' id='state1good'></p></b>");
    wm.addParameter( &label_cold_state);

    WiFiManagerParameter label_cold("<label class='cold label'>Показания холодной воды</label>");
    wm.addParameter( &label_cold);
    FloatParameter param_channel1_start( "ch1", "",  cdata.channel1);
    wm.addParameter( &param_channel1_start);
    
    WiFiManagerParameter param_serial_cold("serialCold", "серийный номер",  sett.serial1, SERIAL_LEN-1);
    wm.addParameter(&param_serial_cold);

    WiFiManagerParameter hot_water("<h3>Горячая вода</h3>");
    wm.addParameter(&hot_water);
            
    WiFiManagerParameter label_hot_info("<p>Откройте кран горячей воды, пока надпись не&nbsp;сменится на&nbsp;&laquo;подключен&raquo;</p>");
    wm.addParameter( &label_hot_info);
    
    WiFiManagerParameter label_hot_state("<b><p class='bad' id='state0bad'></p><p class='good' id='state0good'></p></b>");
    wm.addParameter( &label_hot_state );

    WiFiManagerParameter label_hot("<label class='hot label'>Показания горячей воды</label>");
    wm.addParameter( &label_hot);
    FloatParameter param_channel0_start( "ch0", "",  cdata.channel0);
    wm.addParameter( &param_channel0_start);

    WiFiManagerParameter param_serial_hot("serialHot", "серийный номер",  sett.serial0, SERIAL_LEN-1);
    wm.addParameter(&param_serial_hot);

    wm.setConfigPortalTimeout(SETUP_TIME_SEC);
    wm.setConnectTimeout(ESP_CONNECT_TIMEOUT);
    
    //LOG_INFO(FPSTR(S_AP), F("Start ConfigPortal"));
    log_ap.info(F("Start ConfigPortal"));

    // Запуск веб сервера на 192.168.4.1
    //LOG_INFO(FPSTR(S_AP), F("chip id:") << getChipId());
    log_ap.info(F("chip id:%i"), getChipId());
    
    /*
    String ap_name = AP_NAME "_" + String(getChipId(), HEX).substring(0, 4);
    ap_name.toUpperCase();
    wm.startConfigPortal(ap_name.c_str());
    */
    wm.startConfigPortal(AP_NAME);

    // Успешно подключились к Wi-Fi, можно засыпать
    //LOG_INFO(FPSTR(S_AP), F("Connected to wifi. Save settings, go to sleep"));
    log_ap.info( F("Connected to wifi. Save settings, go to sleep"));

    // Переписываем введенные пользователем значения в Конфигурацию

    strncpy0(sett.waterius_email, param_waterius_email.getValue(), EMAIL_LEN);
    strncpy0(sett.waterius_host, param_waterius_host.getValue(), WATERIUS_HOST_LEN);

    // Генерируем ключ используя и введенную эл. почту
    if (strnlen(sett.waterius_key, WATERIUS_KEY_LEN) == 0) {
        //LOG_INFO(FPSTR(S_CFG), F("Generate waterius key"));
        log_cfg.info( F("Generate waterius key"));
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

    sett.ip = param_ip.getValue();
    sett.gateway = param_gw.getValue();
    sett.mask = param_mask.getValue();

    sett.SaveLogLevel = dropdown_save_log.getValue();
    
    //период отправки данных
    sett.wakeup_per_min = param_wakeup_per.getValue();
    //LOG_INFO(FPSTR(S_AP), "wakeup period, min=" << sett.wakeup_per_min);
    log_ap.info(F("wakeup period, min=%i"), sett.wakeup_per_min);

    //Веса импульсов
    //LOG_INFO(FPSTR(S_AP), "hot dropdown=" << dropdown_hot_factor.getValue());
    //LOG_INFO(FPSTR(S_AP), "cold dropdown=" << dropdown_cold_factor.getValue());
    log_ap.info( F("hot dropdown="), dropdown_hot_factor.getValue());
    log_ap.info( F("cold dropdown="), dropdown_cold_factor.getValue());
    
    uint8_t combobox_factor = dropdown_cold_factor.getValue();
    sett.factor1 = get_factor(combobox_factor, runtime_data.impulses1, data.impulses1, 1);
    
    combobox_factor = dropdown_hot_factor.getValue();
    sett.factor0 = get_factor(combobox_factor, runtime_data.impulses0, data.impulses0, sett.factor1);

    strncpy0(sett.serial0, param_serial_hot.getValue(), SERIAL_LEN);
    strncpy0(sett.serial1, param_serial_cold.getValue(), SERIAL_LEN);

    // Текущие показания счетчиков
    sett.channel0_start = param_channel0_start.getValue();
    sett.channel1_start = param_channel1_start.getValue();

    //sett.liters_per_impuls_hot = 
    //LOG_INFO(FPSTR(S_AP), "factorHot=" << sett.factor0);
    //LOG_INFO(FPSTR(S_AP), "factorCold=" << sett.factor1);
    log_ap.info( F("factorHot=%i"), sett.factor0);
    log_ap.info( F("factorCold=%i"), sett.factor1);

    // Запоминаем кол-во импульсов Attiny соответствующих текущим показаниям счетчиков
    sett.impulses0_start = runtime_data.impulses0;
    sett.impulses1_start = runtime_data.impulses1;

    // Предыдущие показания счетчиков. Вносим текущие значения.
    sett.impulses0_previous = sett.impulses0_start;
    sett.impulses1_previous = sett.impulses1_start;

    //LOG_INFO(FPSTR(S_AP), "impulses0=" << sett.impulses0_start );
    //LOG_INFO(FPSTR(S_AP), "impulses1=" << sett.impulses1_start );
    log_ap.info( F("impulses0=%i"), sett.impulses0_start);
    log_ap.info( F("impulses1=%i"), sett.impulses1_start);

    sett.setup_time = millis();
    
    sett.crc = FAKE_CRC; // todo: сделать нормальный crc16
    storeConfig(sett);
}
