
#include "esp_wifi_types.h"
#include "esp_wifi.h"
#include "ESPAsyncWebServer.h"
#include "WebHandlerImpl.h"
#include <IPAddress.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "AsyncJson.h"
#include "esp_task_wdt.h"
#include "setup.h"
#include "Logging.h"
#include "board.h"
#include "utils.h"
#include "config.h"
#include "wifi_helpers.h"
#include "resources.h"
#include "active_point_api.h"
#include "active_point.h"

#define SETUP_TIME_SEC 			600UL // На какое время Attiny включает ESP (файл Attiny85\src\Setup.h)
#define AP_TASK_STACK_SIZE		(8*1024)
#define AP_TASK_PRIORITY		10


static active_point_state_t active_point_state = active_point_state_t::Idle;
static AsyncWebServer*	server = NULL;
static DNSServer*		dns = NULL;
static TaskHandle_t		task_handle = NULL;
static unsigned long	start_timestamp = 0;
bool 					exit_portal_flag = false;
bool 					start_connect_flag = false;
bool 					factory_reset_flag = false;
wl_status_t 			wifi_connect_status = WL_DISCONNECTED;

const String localIPURL = "http://192.168.4.1";

extern SlaveData data;
extern SlaveData runtime_data;
extern Settings sett;
extern CalculatedData cdata;

String template_bool(const uint8_t value)
{
    if (value > 0)
    {
        return String(F("'value=\"1\" checked"));
    }
    return String();
}

/*
 * Т.к. https://github.com/me-no-dev/ESPAsyncWebServer/issues/1249
 */
String replace_value(const String &var)
{
    String out(var);
    out.replace(F("%"), F("%%"));
    return out;
}

String get_counter_img(const uint8_t input, const uint8_t name, const uint8_t ctype)
{
    if (ctype == CounterType::HALL) 
    {
        switch (input)
        {
            case 0: return F("meter-hall-0.png");
            case 1: return F("meter-hall-1.png");
        }
        
    }
    else if (ctype == CounterType::ELECTRONIC)
    {
        switch (input)
        {
            case 0: return F("meter-electro-0.png");
            case 1: return F("meter-electro-1.png");
        }
    }
    else if (name == CounterName::WATER_HOT)
    {
        switch (input)
        {
            case 0: return F("meter-hot-0.png");
            case 1: return F("meter-hot-1.png");
        }
    }
    else if (name == CounterName::GAS)
    {
        switch (input)
        {
            case 0: return F("meter-gas-0.png");
            case 1: return F("meter-gas-1.png");
        }
    }
    //if (name == CounterName::WATER_COLD)
    switch (input)
    {
        case 0: 
            return F("meter-cold-0.png");
        case 1: 
        default:
            return F("meter-cold-1.png");
    }
}

String processor0(const String &var)
{
    return processor_main(var, 0);
}

String processor1(const String &var)
{
    return processor_main(var, 1);
}

String processor(const String &var)
{
    return processor_main(var);
}

String processor_main(const String &var, const uint8_t input)
{   
    if (var == FPSTR(PARAM_VERSION))
        return String(runtime_data.version);

    else if (var == FPSTR(PARAM_VERSION_ESP))
        return FIRMWARE_VERSION;

    else if (var == FPSTR(PARAM_WATERIUS_HOST))
        return replace_value(sett.waterius_host);
    else if (var == FPSTR(PARAM_WATERIUS_EMAIL))
    {
        if (!strstr(sett.waterius_email, "@waterius.ru"))
            return replace_value(sett.waterius_email);
    }
    else if (var == FPSTR(PARAM_HTTP_URL))
        return replace_value(sett.http_url);

    else if (var == FPSTR(PARAM_MQTT_HOST))
        return replace_value(sett.mqtt_host);
    else if (var == FPSTR(PARAM_MQTT_PORT))
        return String(sett.mqtt_port);
    else if (var == FPSTR(PARAM_MQTT_LOGIN))
        return replace_value(sett.mqtt_login);
    else if (var == FPSTR(PARAM_MQTT_PASSWORD))
        return replace_value(sett.mqtt_password);
    else if (var == FPSTR(PARAM_MQTT_TOPIC))
        return replace_value(sett.mqtt_topic);

    // на вебстраницах входа
    else if (var == FPSTR(PARAM_INPUT))
        return String(input);
        
    else if (var == FPSTR(PARAM_CHANNEL_START))
    {
        switch (input)
        {
            case 0: return String(sett.channel0_start);
            case 1: return String(sett.channel1_start);
        }
    }
    else if (var == FPSTR(PARAM_SERIAL))
    {
        switch (input)
        {
            case 0: return replace_value(sett.serial0);
            case 1: return replace_value(sett.serial1);
        }
    }

    else if (var == FPSTR(PARAM_COUNTER_NAME))
    {
        switch (input)
        {
            case 0: return String(sett.counter0_name);
            case 1: return String(sett.counter1_name);
        }
    }

    else if (var == FPSTR(PARAM_COUNTER0_NAME))
    {
        return String(sett.counter0_name);
    }

    else if (var == FPSTR(PARAM_COUNTER1_NAME))
    {
        return String(sett.counter1_name);
    }

    else if (var == FPSTR(PARAM_COUNTER_IMG))
    {
        switch (input)
        {
            case 0: return get_counter_img(0, sett.counter0_name, runtime_data.counter_type0);
            case 1: return get_counter_img(1, sett.counter1_name, runtime_data.counter_type1);
        }
    }

    else if (var == FPSTR(PARAM_COUNTER_TYPE))
    {
        switch (input)
        {
            case 0: return String(runtime_data.counter_type0);
            case 1: return String(runtime_data.counter_type1);
        }
    }

    else if (var == FPSTR(PARAM_COUNTER0_TYPE))
    {
        return String(runtime_data.counter_type0);
    }

    else if (var == FPSTR(PARAM_COUNTER1_TYPE))
    {
        return String(runtime_data.counter_type1);
    }

    else if (var == FPSTR(PARAM_FACTOR))
    {
        switch (input)
        {
            case 0: return sett.factor0 == AS_COLD_CHANNEL ? F("10") : String(sett.factor0);
            case 1: return sett.factor1 == AUTO_IMPULSE_FACTOR ? F("10") : String(sett.factor1);
        }
    }

    else if (var == FPSTR(PARAM_IP))
        return IPAddress(sett.ip).toString();
    else if (var == FPSTR(PARAM_GATEWAY))
        return IPAddress(sett.gateway).toString();
    else if (var == FPSTR(PARAM_MASK))
        return IPAddress(sett.mask).toString();
    else if (var == FPSTR(PARAM_MAC_ADDRESS))
        return WiFi.macAddress();

    else if (var == FPSTR(PARAM_WAKEUP_PER_MIN))
        return String(sett.wakeup_per_min);

    else if (var == FPSTR(PARAM_MQTT_AUTO_DISCOVERY))
        return template_bool(sett.mqtt_auto_discovery);
    else if (var == FPSTR(PARAM_MQTT_DISCOVERY_TOPIC))
        return replace_value(sett.mqtt_discovery_topic);

    else if (var == FPSTR(PARAM_NTP_SERVER))
        return String(sett.ntp_server);

    else if (var == FPSTR(PARAM_SSID))
        return replace_value(sett.wifi_ssid);
    else if (var == FPSTR(PARAM_PASSWORD))
        return replace_value(sett.wifi_password);

    else if (var == FPSTR(PARAM_WIFI_PHY_MODE))
        return String(sett.wifi_phy_mode);

    else if (var == FPSTR(PARAM_WATERIUS_ON))
        return template_bool(sett.waterius_on);
    else if (var == FPSTR(PARAM_HTTP_ON))
        return template_bool(sett.http_on);
    else if (var == FPSTR(PARAM_MQTT_ON))
        return template_bool(sett.mqtt_on);
    else if (var == FPSTR(PARAM_DHCP_OFF))
        return template_bool(sett.dhcp_off);

    //
    else if (var == FPSTR(PARAM_BUILD_DATE_TIME))
        return String(__DATE__) + String(" ") + String(__TIME__);
    else if (var == FPSTR(PARAM_FS_SIZE))
        return String(LittleFS.totalBytes());
    else if (var == FPSTR(PARAM_FS_FREE))
        return String(LittleFS.totalBytes() - LittleFS.usedBytes());
    else if (var == FPSTR(PARAM_WIFI_CONNECT_STATUS))
    {   
        switch (wifi_connect_status)
        {   
            case WL_NO_SSID_AVAIL:
            case WL_CONNECT_FAILED:
            case WL_CONNECTION_LOST:
                return String(F("8")); //S_WIFI_CONNECTION_LOST "Ошибка подключения. Попробуйте ещё раз.<br>Если не помогло, то пропишите статический ip. Еще можно зарезервировать MAC адрес Ватериуса в роутере. Если ничего не помогло, пришлите нам <a class='link' href='http://192.168.4.1/ssid.txt'>файл</a> параметров wi-fi сетей.";
            //case WL_WRONG_PASSWORD:
            //   return String(F("9")); //S_WL_WRONG_PASSWORD "Ошибка подключения: Некорректный пароль";
            case WL_IDLE_STATUS:
                return String(F("10")); //S_WL_IDLE_STATUS "Ошибка подключения: Код 0";
            case WL_STOPPED:
            case WL_DISCONNECTED:
                return String(F("11")); //S_WL_DISCONNECTED "Ошибка подключения: Отключен";
            case WL_NO_SHIELD:
                return String(F("12")); //S_WL_NO_SHIELD "Ошибка подключения: Код 255";
            case WL_SCAN_COMPLETED:
                return String(F("13")); //S_WL_SCAN_COMPLETED "Ошибка подключения: Код 2";
            case WL_CONNECTED:
                break;
        }
    }    

    return String();
}

void onNotFound(AsyncWebServerRequest *request)
{
    LOG_INFO(F("onNotFound ") << request->host() << request->url());
    request->send(404);
};

void onRedirectIP(AsyncWebServerRequest *request)
{
    LOG_INFO(F("redirect  ") << request->host() << request->url());
    request->redirect(localIPURL);
}

void on_root(AsyncWebServerRequest *request)
{
    LOG_INFO(F("on_root GET ") << request->url());

    LOG_INFO(F("WIFI: wifi_connect_status=") << (int)wifi_connect_status);
	delay(50);

    if (sett.factor1 == AUTO_IMPULSE_FACTOR)
    {
        if (wifi_connect_status == WL_CONNECT_FAILED || wifi_connect_status == WL_CONNECTION_LOST)
        {
            LOG_INFO(F("> captive_portal_error.html"));
            request->send(LittleFS, "/captive_portal_error.html", F("text/html"), false);
        }
        else if (wifi_connect_status == WL_CONNECTED)
        {
            LOG_INFO(F("> captive_portal_connected.html"));
            request->send(LittleFS, "/captive_portal_connected.html", F("text/html"), false);
        }   
        else 
        {
            // Первая настройка
            LOG_INFO(F("> captive_portal_start.html"));
            request->send(LittleFS, "/captive_portal_start.html", F("text/html"), false);
        }
    }
    else
    {
        LOG_INFO(F("> captive_portal.html"));
        request->send(LittleFS, "/captive_portal.html", F("text/html"), false);
    }
}

bool setup_active_point()
{   
    //Т.к. интерфейс берёт данные из runtime_data, то туда нужно загрузить их
    runtime_data = data;

    if (!LittleFS.begin())
    {
        LOG_INFO(F("FS: Mounting LittleFS error"));
        return false;
    }
    LOG_INFO(F("FS: LittleFS mounted"));

    LOG_INFO(F("FS: ") << LittleFS.totalBytes() << F(" bytes, size"));
    LOG_INFO(F("FS: ") << LittleFS.totalBytes() - LittleFS.usedBytes() << F(" bytes, used"));

    // Если настройки есть в конфиге то присваиваем их
    if (sett.wifi_ssid[0])
    {
        LOG_INFO(F("Apply SSID:") << String(sett.wifi_ssid) << F(" from config"));
        wifi_config_t current_conf;
        esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf);
        memset(current_conf.sta.ssid, 0, sizeof(current_conf.sta.ssid));
        memset(current_conf.sta.password, 0, sizeof(current_conf.sta.password));

        memcpy(current_conf.sta.ssid, sett.wifi_ssid, sizeof(current_conf.sta.ssid));
        if (sett.wifi_password[0])
        {
            memcpy(current_conf.sta.password, sett.wifi_password, sizeof(current_conf.sta.password));
            LOG_INFO(F("Apply password from config"));
        }
        else
        {
            current_conf.sta.password[0] = 0;
            LOG_INFO(F("No password in config"));
        }
        esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf);
    }
    else
    {
        LOG_INFO(F("No SSID saved in config"));
    }

    wifi_set_mode(WIFI_AP_STA);

    // WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

    // TODO выбирать channel исходя из настроек.
    // Канал WiFi роутера к кому подсоединимся должен совпадать с каналом точки доступа ESP
    // https://bbs.espressif.com/viewtopic.php?t=324
    // TODO добавить пароль для интерфейса
    if (!WiFi.softAP(get_ap_name(), "", sett.wifi_channel, 0, 4))
    {
        LOG_ERROR(F("AP started failed"));
        return false;
    }

    delay(500);

    LOG_INFO(F("AP started on channel=") << sett.wifi_channel << F(" , ssid=") << get_ap_name());
    LOG_INFO(F("IP: ") << WiFi.softAPIP());

    LOG_INFO(F("Start DNS server"));
    dns = new DNSServer();
    // dns->setTTL(3600);   //https://github.com/CDFER/Captive-Portal-ESP32/blob/main/src/main.cpp#L50C11-L50C25
    dns->start(53, "*", WiFi.softAPIP());

    LOG_INFO(F("DNS server started"));

    LOG_INFO(F("Start HTTP server"));
    server = new AsyncWebServer(80);

    server->onNotFound(onNotFound);

    // Главная страница
    server->on("/", HTTP_GET, on_root).setFilter(ON_AP_FILTER);

    // CaptivePortal
    // https://github.com/CDFER/Captive-Portal-ESP32/blob/main/src/main.cpp#L50C11-L50C25

    //======================== Webserver ========================
    // WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
    // SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
    // SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
    // SAFARI (IOS) popup browser has some severe limitations (javascript disabled, cookies disabled)

    // Required
    server->on("/connecttest.txt", [](AsyncWebServerRequest *request)
               {
                   LOG_INFO(request->url());
                   request->redirect("http://logout.net");
               });                       // windows 11 captive portal workaround
    server->on("/wpad.dat", onNotFound); // Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

    // Background responses: Probably not all are Required, but some are. Others might speed things up?
    // A Tier (commonly used by modern systems)
    server->on("/generate_204", onRedirectIP);        // android captive portal redirect
    server->on("/redirect", onRedirectIP);            // microsoft redirect
    server->on("/hotspot-detect.html", onRedirectIP); // apple call home
    server->on("/canonical.html", onRedirectIP);      // firefox captive portal call home
    server->on("/success.txt", onRedirectIP);         // firefox captive portal call home
    server->on("/ncsi.txt", onRedirectIP);            // windows call home
    server->on("/fwlink", HTTP_GET, on_root);          // Microsoft captive portal. 

    // TODO добавить .setLastModified( и  https://github.com/GyverLibs/buildTime/releases/tag/1.0
    server->serveStatic("/images/", LittleFS, "/images/").setCacheControl("max-age=600");
    server->serveStatic("/static/", LittleFS, "/static/").setCacheControl("max-age=600");

    // Об устройстве
    server->on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/about.html", F("text/html"), false, processor); });

    server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/favicon.ico", F("image/x-icon")); });

    // Captive portal
    // Он отобразится через / on_root, но кажется кнопки "назад" поведут в эти хэндлеры
    server->on("/captive_portal.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/captive_portal.html", F("text/html"), false); });

    server->on("/captive_portal_start.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/captive_portal_start.html", F("text/html"), false); });

    server->on("/captive_portal_error.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/captive_portal_error.html", F("text/html"), false); });

    server->on("/captive_portal_connected.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/captive_portal_connected.html", F("text/html"), false); });

    // Завершение настройки (wizard)
    server->on("/finish.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/finish.html", F("text/html"), false, processor); });

    // Нужно, т.к. при первой на стройке будет start.html и надо дать возможность открыть Главное меню
    server->on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/index.html", F("text/html"), false, processor); });

    server->on("/logs.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/logs.html", F("text/html"), false, processor); });

    // Сброс к заводским настройкам
    server->on("/reset.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/reset.html", F("text/html"), false, processor); });

    // Настройка счётчика

    // красный
    server->on("/input/0/setup.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_setup.html", F("text/html"), false, processor0); });
    // синий
    server->on("/input/1/setup.html", HTTP_GET, [](AsyncWebServerRequest *request)  
               { request->send(LittleFS, "/input_setup.html", F("text/html"), false, processor1); });

    // Детектирование счётчика (только Холодная и Горячая вода)
    server->on("/input/0/detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_detect.html", F("text/html"), false, processor0); });
    server->on("/input/1/detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_detect.html", F("text/html"), false, processor1); });

    server->on("/input/0/hall_detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_hall_detect.html", F("text/html"), false, processor0); });
    server->on("/input/1/hall_detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_hall_detect.html", F("text/html"), false, processor1); });

    // Параметры счётчика
    server->on("/input/0/settings.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_settings.html", F("text/html"), false, processor0); });
    server->on("/input/1/settings.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_settings.html", F("text/html"), false, processor1); });
               
    server->on("/input/0/hall_settings.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_hall_settings.html", F("text/html"), false, processor0); });
    server->on("/input/1/hall_settings.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/input_hall_settings.html", F("text/html"), false, processor1); });

    // Отправка показаний
    server->on("/setup_send.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/setup_send.html", F("text/html"), false, processor); });

    // Первая настройка
    server->on("/start.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/start.html", F("text/html"), false, processor); });

    server->on("/waterius_logs.txt", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/waterius_logs.txt", F("text/plain")); });

    // Процесс подключения к Wi-fi
    server->on("/wifi_connect.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_connect.html", F("text/html"), false, processor); });

    // Список Wi-Fi сетей
    server->on("/wifi_list.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_list.html", F("text/html"), false, processor); });

    // Ввод пароля от Wi-Fi сети
    server->on("/wifi_password.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_password.html", F("text/html"), false, processor); });

    // Настройки Wi-Fi, NTP
    server->on("/wifi_settings.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/wifi_settings.html", F("text/html"), false, processor); });

    // Файл характеристик всех найденных Wi-Fi сетей
    server->on("/ssid.txt", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/ssid.txt", F("text/plain")); });

    /*API*/
    server->on("/api/networks", HTTP_GET, get_api_networks);                       // Список Wi-Fi сетей (из wifi_list.html)
    server->on("/api/save_connect", HTTP_POST, post_api_save_connect);             // Сохраняем настройки Wi-Fi + redirect: /api/connect
    server->on("/api/start_connect", HTTP_GET, get_api_start_connect);             // Поднимаем флаг старта подключения и redirect в wifi_connect.html
    server->on("/api/connect_status", HTTP_GET, get_api_connect_status);           // Статус подключения (из wifi_connect.html)
    server->on("/api/save", HTTP_POST, post_api_save);                             // Сохраняем настройки
    server->on("/api/save_input_type", HTTP_POST, post_api_save_input_type);       // Сохраняем тип счётчика и переносим на страницу настройки
    server->on("/api/main_status", HTTP_GET, get_api_main_status);                 // Информационные сообщения на главной странице
    server->on("/api/status/0", HTTP_GET, get_api_status_0);                       // Статус 0-го входа (ХВС)  (из setup_cold_welcome.html)
    server->on("/api/status/1", HTTP_GET, get_api_status_1);                       // Статус 1-го входа (ГВС)  (из setup_cold_welcome.html)
    server->on("/api/turnoff", HTTP_GET, get_api_turnoff);                         // Выйти из режима настройки
    server->on("/api/reset", HTTP_POST, post_api_reset);                           // Сброс к заводским настройкам

    server->begin();

    LOG_INFO(F("HTTP server started"));

    // Начинаем сканирование Wi-Fi сетей
    LOG_INFO(F("Start scan Wi-Fi networks"));
    WiFi.scanNetworks(true);

	return true;
}

static void ap_task(void* pvParameters)
{
	esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 3000,
        .idle_core_mask = (1 << CONFIG_SOC_CPU_CORES_NUM) - 1,    // Bitmask of all cores
        .trigger_panic = false,
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure (&twdt_config));

	active_point_state = active_point_state_t::Start;
	if (setup_active_point()) {
		active_point_state = active_point_state_t::Run;
		start_timestamp = millis();
	} else {
		active_point_state = active_point_state_t::Error;
	}

	while (active_point_state == active_point_state_t::Run) {
		dns->processNextRequest();
		if (start_connect_flag)	{
			wifi_connect(sett, WIFI_AP_STA);
			wifi_connect_status = WiFi.status();
			start_connect_flag = false;
		}
		if (factory_reset_flag)	{
			factory_reset(sett);
		}
		if (exit_portal_flag) {
			active_point_state = active_point_state_t::Stop;
		}
		if (((millis() - start_timestamp) / 1000) > SETUP_TIME_SEC)	{
			LOG_ERROR(F("Portal setup time is over"));
			active_point_state = active_point_state_t::Stop;
		}

		vTaskDelay(pdMS_TO_TICKS(50));
	}

	if (active_point_state == active_point_state_t::Stop) {
		LOG_INFO(F("Shutdown HTTP and DNS servers"));
		server->end();
		dns->stop();
		delete server;
		delete dns;
		active_point_state = active_point_state_t::Finish;
	}
}

int start_active_point()
{
	if (active_point_state != active_point_state_t::Idle)
		return ESP_ERR_INVALID_STATE;
	// Создаем поток AP
    BaseType_t err = xTaskCreate(
		ap_task,
		"active point",
		AP_TASK_STACK_SIZE,
		0,
		AP_TASK_PRIORITY,
		&task_handle);
	if (err != pdTRUE) {
		active_point_state = active_point_state_t::Error;
		LOG_ERROR(F("Starting AP task failed"));
		return ESP_ERR_INVALID_RESPONSE;
	}
}

active_point_state_t active_point()
{
	if (sett.mode == SETUP_MODE) {
		if (active_point_state == active_point_state_t::Idle) {
			start_active_point();
		}
	}
	if (active_point_state == active_point_state_t::Finish) {
		task_handle = NULL;
	}
	return active_point_state;
}
