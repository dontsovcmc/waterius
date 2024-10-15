#include "active_point_api.h"
#ifdef ESP8266
#include "ESPAsyncTCP.h"
#endif
#include "WebHandlerImpl.h"
#include <IPAddress.h>
#include <LittleFS.h>
#include "AsyncJson.h"

#include "setup.h"
#include "Logging.h"
#include "master_i2c.h"
#include "utils.h"
#include "config.h"
#include "wifi_helpers.h"
#include "resources.h"

extern bool exit_portal_flag;
extern bool start_connect_flag;
extern wl_status_t wifi_connect_status;
extern bool factory_reset_flag;

SlaveData runtime_data;
extern SlaveData data;
extern MasterI2C masterI2C;
extern Settings sett;
extern CalculatedData cdata;

#define IMPULS_LIMIT_1 3 // Если пришло импульсов меньше 3, то перед нами 10л/имп. Если больше, то 1л/имп.

uint8_t get_auto_factor(const uint32_t runtime_impulses, const uint32_t impulses)
{
    return (runtime_impulses - impulses <= IMPULS_LIMIT_1) ? 10 : 1;
}

/**
 * @brief Запрос состояния подключения к роутеру.
 *        После успеха или не успеха - переадресация на другую страницу.
 *
 * @param request запрос
 */
void get_api_connect_status(AsyncWebServerRequest *request)
{
    LOG_INFO(F("GET ") << request->url());

    DynamicJsonDocument json_doc(JSON_SMALL_STATIC_MSG_BUFFER);
    JsonObject ret = json_doc.to<JsonObject>();

    if (start_connect_flag)
    {
        //ret["status"] = F("4");  // S_CONNECTING "выполняется подключение..." not used?
        LOG_INFO(F("WIFI: connecting..."));
    }
    else
    {
        LOG_INFO(F("WIFI: wifi_connect_status=") << wifi_connect_status);

        if (wifi_connect_status == WL_CONNECTED)
        {
            ret[F("redirect")] = F("/input/1/setup.html");
        }
        else
        {
            ret[F("redirect")] = F("/wifi_settings.html");
        }
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(json_doc, *response);
    request->send(response);
};

/**
 * @brief Список Wi-Fi сетей
 *
 * @param request запрос
 */
void get_api_networks(AsyncWebServerRequest *request)
{
    LOG_INFO(F("GET ") << request->url());

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED)
    {
        WiFi.scanNetworks(true);
        request->send(200, "", F("[]"));
    }
    else if (n)
    {
        DynamicJsonDocument json_doc(JSON_DYNAMIC_MSG_BUFFER);
        JsonArray array = json_doc.to<JsonArray>();

        for (int i = 0; i < n; ++i)
        {
            LOG_INFO(WiFi.SSID(i) << " " << WiFi.RSSI(i));
            JsonObject obj = array.createNestedObject();
            obj["ssid"] = WiFi.SSID(i);
            obj["level"] = int(round(map(WiFi.RSSI(i), -100, -50, 1, 4)));
            obj["wifi_channel"] = WiFi.channel();
        }

        write_ssid_to_file();

        WiFi.scanDelete();

        AsyncResponseStream *response = request->beginResponseStream("application/json");
        serializeJson(json_doc, *response);
        request->send(response);
    }
};

/**
 * @brief Подключение к точки доступа
 *
 * @param request запрос
 */
void post_api_save_connect(AsyncWebServerRequest *request)
{
    LOG_INFO(F("POST ") << request->url());

    DynamicJsonDocument json_doc(JSON_SMALL_STATIC_MSG_BUFFER);
    JsonObject ret = json_doc.to<JsonObject>();
    JsonObject errorsObj = ret.createNestedObject(F("errors"));

    // Если канал WiFi отличен от текущего канала AP ESP, то возможно отключение телефона
    uint8_t channel = sett.wifi_channel;

    applySettings(request, errorsObj);
    
    bool wizard = find_wizard_param(request);
    
    if (!errorsObj.size())
    {
        ret.remove(F("errors"));

        String params;

        if (channel != sett.wifi_channel)
        {
            if (wizard) 
            {
                params += F("wizard=true&error=0");  // ERROR_ANOTHER_CHANNEL "Канал Wi-Fi роутера отличается от текущего соединения. Если телефон потеряет связь с Ватериусом, подключитесь заново." 
            }
            else 
            {
                params += F("error=0"); // ERROR_ANOTHER_CHANNEL
            }
        }
        else 
        {
            if (wizard) 
            {
                params += F("wizard=true");
            }
        }

        if (params.length())
        {
            ret[F("redirect")] = String(F("/api/start_connect?")) + params;
        }
        else 
        {
            ret[F("redirect")] = F("/api/start_connect");
        }
    }
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(json_doc, *response);
    request->send(response);
}

/**
 * @brief Подключение к точки доступа
 *
 * @param request запрос
 */
void get_api_start_connect(AsyncWebServerRequest *request)
{
    start_connect_flag = true;
    wifi_connect_status = WL_DISCONNECTED;
    LOG_INFO(F("Start connect"));

    bool wizard = find_wizard_param(request);
    if (wizard)
    {
        request->redirect("/wifi_connect.html?wizard=true");
    }
    else
    {
        request->redirect("/wifi_connect.html");
    }
}

/**
 * @brief Список диагностических сообщений на Главной странице вебсервера
 *        
 * @param request запрос
 */
void get_api_main_status(AsyncWebServerRequest *request)
{
    LOG_INFO(F("GET ") << request->url());

    DynamicJsonDocument json_doc(JSON_SMALL_STATIC_MSG_BUFFER);
    JsonArray array = json_doc.to<JsonArray>();

    wl_status_t status = WiFi.status();
    LOG_INFO(F("WIFI: status=") << status);
    
    if (status == WL_CONNECT_FAILED || status == WL_CONNECTION_LOST)
    {
        JsonObject obj = array.createNestedObject();
        obj["error"] = F("1");  // S_WIFI_CONNECT "Ошибка подключения к Wi-Fi"
        obj["link_text"] = F("5"); // S_SETUP Настроить
        obj["link"] = String(F("/wifi_settings.html?status_code=")) + String(status);
    }
    else
    {
        if (sett.factor1 == AUTO_IMPULSE_FACTOR)
        {
            if (status == WL_CONNECTED)
            {
                JsonObject obj = array.createNestedObject();
                obj["error"] = F("2");  // S_SETUP_COUNTERS "Ватериус успешно подключился к Wi-Fi. Теперь настроим счётчики."
                obj["link_text"] = F("5"); // S_SETUP Настроить
                obj["link"] = F("/input/1/setup.html");
            }
            else 
            {
                JsonObject obj = array.createNestedObject();
                obj["error"] = F("3");  // S_NEED_SETUP "Ватериус ещё не настроен"
                obj["link_text"] = F("6"); // S_LETSGO Приступить
                obj["link"] = F("/captive_portal_start.html");
            }
        }
    }

    LOG_INFO(F("JSON: Mem usage: ") << json_doc.memoryUsage());
    LOG_INFO(F("JSON: Size: ") << measureJson(json_doc));

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(json_doc, *response);
    request->send(response);
}

void get_api_status_0(AsyncWebServerRequest *request)
{
    get_api_status(request, 0);
}

void get_api_status_1(AsyncWebServerRequest *request)
{
    get_api_status(request, 1);
}

/**
 * @brief Запрос состояния входа
 *
 * @param request запрос
 */
void get_api_status(AsyncWebServerRequest *request, const int index)
{
    LOG_INFO(F("GET ") << request->url());

    DynamicJsonDocument json_doc(JSON_SMALL_STATIC_MSG_BUFFER);
    JsonObject ret = json_doc.to<JsonObject>();

    uint16_t factor;
    if (masterI2C.getSlaveData(runtime_data))
    {
        if (index == 0)
        {
            if (sett.factor0 == AS_COLD_CHANNEL)
            {
                if (sett.factor1 == AUTO_IMPULSE_FACTOR)
                {
                    factor = get_auto_factor(runtime_data.impulses0, data.impulses0);
                }
                else
                {
                    factor = sett.factor1;
                }
            }
            else
            {
                factor = sett.factor0;
            }
            ret[F("state")] = int(runtime_data.impulses0 > data.impulses0);
            ret[F("factor")] = factor;
            ret[F("impulses")] = runtime_data.impulses0 - data.impulses0;
        }
        else if (index == 1)
        {
            if (sett.factor1 == AUTO_IMPULSE_FACTOR)
            {
                factor = get_auto_factor(runtime_data.impulses1, data.impulses1);
            }
            else // повторная настройка
            {
                factor = sett.factor1;
            }
            ret[F("state")] = int(runtime_data.impulses1 > data.impulses1);
            ret[F("factor")] = factor;
            ret[F("impulses")] = runtime_data.impulses1 - data.impulses1;
        }
        // root[F("elapsed")] = (uint32_t)(SETUP_TIME_SEC - millis() / 1000.0);
    }
    else
    {
        ret[F("error")] = F("7"); // S_NO_LINK Ошибка связи с МК
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(json_doc, *response);
    request->send(response);
};

/**
 * @brief Запрос сохранения настроек
 *
 * @param request POST, данные в x-www-form-urlencoded
 *
 *      Удаляем поля где значение null
 *      Проверяем настройки на корректность
 *      :param form_data: dict
 *      :return:
 *      {...form_data...} - успех
 *
 *      Если есть ошибки:
 *      {...form_data...
 *          "errors": {
 *              "serial": "ошибка"
 *          }
 *      }
 */

void save_param(AsyncWebParameter *p, char *dest, size_t size, JsonObject &errorsObj, bool required /*true*/)
{
    if (p->value().length() >= size)
    {
        LOG_ERROR(FPSTR(ERROR_LENGTH_63) << ": " << p->name());
        errorsObj[p->name()] = FPSTR(ERROR_LENGTH_63);
    }
    else if (required && p->value().length() == 0)
    {
        LOG_ERROR(FPSTR(ERROR_EMPTY) << ": " << p->name());
        errorsObj[p->name()] = FPSTR(ERROR_EMPTY);
    }
    else
    {   
        String value(p->value());
        value.trim();  //чтобы пользователи случайно не ввели пробел
        strncpy0(dest, value.c_str(), size);
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << value);
    }
}

void save_param(AsyncWebParameter *p, uint16_t &v, JsonObject &errorsObj)
{
    if (p->value().toInt() == 0)
    {
        LOG_ERROR(FPSTR(ERROR_VALUE) << ": " << p->name());
        errorsObj[p->name()] = FPSTR(ERROR_VALUE);
    }
    else
    {
        v = p->value().toInt();
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    }
}

void save_param(AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj, const bool zero_ok)
{
    if (!zero_ok && p->value().toInt() == 0)
    {
        LOG_ERROR(FPSTR(ERROR_VALUE) << ": " << p->name());
        errorsObj[p->name()] = FPSTR(ERROR_VALUE);
    }
    else
    {
        v = p->value().toInt();
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    }
}

void save_bool_param(AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj)
{
    if (p->value().toInt() > 1)
    {
        LOG_ERROR(FPSTR(ERROR_VALUE) << ": " << p->name());
        errorsObj[p->name()] = FPSTR(ERROR_VALUE);
    }
    else
    {
        v = p->value().toInt();
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    }
}

void save_param(AsyncWebParameter *p, float &v, JsonObject &errorsObj)
{
    /* Позволяем вводить 0.0 у счётчиков.
    if (p->value().toFloat() == 0.0)
    {
        LOG_ERROR(FPSTR(ERROR_VALUE) << ": " << p->name());
        errorsObj[p->name()] = FPSTR(ERROR_VALUE);
    }
    else */
    {
        v = p->value().toFloat();
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    }
}

void save_ip_param(AsyncWebParameter *p, uint32_t &v, JsonObject &errorsObj)
{
    IPAddress ip;
    if (ip.fromString(p->value()))
    {
        v = ip;
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << ip.toString());
    }
    else
    {
        LOG_ERROR(FPSTR(ERROR_VALUE) << ": " << p->name());
        errorsObj[p->name()] = FPSTR(ERROR_VALUE);
    }
}

bool find_wizard_param(AsyncWebServerRequest *request)
{
    for (size_t i = 0; i < request->params(); i++)
    {
        AsyncWebParameter *p = request->getParam(i);
        if (p->name() == FPSTR(PARAM_WIZARD))
        {
            return p->value() == FPSTR(PARAM_TRUE);
        }
    }
    return false;
}

uint8_t get_param_uint8(AsyncWebServerRequest *request, const String &name)
{
    for (size_t i = 0; i < request->params(); i++)
    {
        AsyncWebParameter *p = request->getParam(i);
        if (p->name() == name)
        {
            return p->value().toInt();
        }
    }
    return 0xFF;
}

void applyInputSettings(AsyncWebServerRequest *request, JsonObject &errorsObj, const uint8_t input)
{
    const int params = request->params();

    LOG_INFO(F("Apply Input ") << params << " parameters");

    for (int i = 0; i < params; i++)
    {
        AsyncWebParameter *p = request->getParam(i);
        const String &name = p->name();
        
        LOG_INFO(F("parameter ") << name << "=" << p->value());
        if (name == FPSTR(PARAM_CHANNEL_START))
        {
            switch (input) 
            {
                case 0: 
                    save_param(p, sett.channel0_start, errorsObj);
                    sett.impulses0_start = runtime_data.impulses0;
                    sett.impulses0_previous = sett.impulses0_start;
                    LOG_INFO("impulses0_start=" << sett.impulses0_start);
                    break;
                case 1:
                    save_param(p, sett.channel1_start, errorsObj);
                    sett.impulses1_start = runtime_data.impulses1;
                    sett.impulses1_previous = sett.impulses1_start;
                    LOG_INFO("impulses1_start=" << sett.impulses1_start);
                    break;
            }
        }
        else if (name == FPSTR(PARAM_SERIAL))
        {
            switch (input)
            {
                case 0: 
                    save_param(p, sett.serial0, SERIAL_LEN, errorsObj, false);
                    break;
                case 1: 
                    save_param(p, sett.serial1, SERIAL_LEN, errorsObj, false);
                    break;
            }
        }
        else if (name == FPSTR(PARAM_COUNTER_NAME))
        {   
            switch(input) 
            {
                case 0: 
                    save_param(p, sett.counter0_name, errorsObj, true);
                    break;
                case 1:
                    save_param(p, sett.counter1_name, errorsObj, true);
                    break;
            }
            
        }
        else if (name == FPSTR(PARAM_COUNTER_TYPE))
        {   
            switch (input) 
            {
                case 0:
                    if (!masterI2C.setCountersType(p->value().toInt(), runtime_data.counter_type1))
                    {
                        LOG_ERROR(FPSTR(ERROR_ATTINY_ERROR) << ": " << p->name());
                        errorsObj[p->name()] = FPSTR(ERROR_ATTINY_ERROR);
                    }
                    else
                    {
                        runtime_data.counter_type0 = p->value().toInt();
                        LOG_INFO(FPSTR(PARAM_SAVED0) << p->name() << F("=") << p->value());
                    }
                    break;
                case 1:
                    if (!masterI2C.setCountersType(runtime_data.counter_type0, p->value().toInt()))
                    {
                        LOG_ERROR(FPSTR(ERROR_ATTINY_ERROR) << ": " << p->name());
                        errorsObj[p->name()] = FPSTR(ERROR_ATTINY_ERROR);
                    }
                    else
                    {
                        runtime_data.counter_type1 = p->value().toInt();
                        LOG_INFO(FPSTR(PARAM_SAVED1) << p->name() << F("=") << p->value());
                    }
                    break;
            }
            
        }
        else if (name == FPSTR(PARAM_FACTOR))
        {
            switch (input) 
            {
                case 0: 
                    save_param(p, sett.factor0, errorsObj);
                    break;
                case 1:
                    save_param(p, sett.factor1, errorsObj);
                    break;
            }
        }
    }

    store_config(sett);
}

void applySettings(AsyncWebServerRequest *request, JsonObject &errorsObj)
{
    const int params = request->params();

    LOG_INFO(F("Apply ") << params << " parameters");

    // Вначале bool, чтобы дальше проверять только требуемые параметры
    for (int i = 0; i < params; i++)
    {
        AsyncWebParameter *p = request->getParam(i);
        const String &name = p->name();

        LOG_INFO(F("parameter ") << name << "=" << p->value());
        if (name == FPSTR(PARAM_WATERIUS_ON))
        {
            save_bool_param(p, sett.waterius_on, errorsObj);
        }
        else if (name == FPSTR(PARAM_HTTP_ON))
        {
            save_bool_param(p, sett.http_on, errorsObj);
        }
        else if (name == FPSTR(PARAM_MQTT_ON))
        {
            save_bool_param(p, sett.mqtt_on, errorsObj);
        }
        else if (name == FPSTR(PARAM_DHCP_OFF))
        {
            save_bool_param(p, sett.dhcp_off, errorsObj);
        }
        else if (name == FPSTR(PARAM_MQTT_AUTO_DISCOVERY))
        {
            save_bool_param(p, sett.mqtt_auto_discovery, errorsObj);
        }
    }

    for (int i = 0; i < params; i++)
    {
        AsyncWebParameter *p = request->getParam(i);
        const String &name = p->name();

        if (sett.waterius_on)
        {
            if (name == FPSTR(PARAM_WATERIUS_HOST))
            {
                save_param(p, sett.waterius_host, HOST_LEN, errorsObj);
            }
            else if (name == FPSTR(PARAM_WATERIUS_EMAIL))
            {
                save_param(p, sett.waterius_email, EMAIL_LEN, errorsObj);
            }
        }
        
        if (sett.http_on)
        {
            if (name == FPSTR(PARAM_HTTP_URL))
            {
                save_param(p, sett.http_url, HOST_LEN, errorsObj);
            }
        }
        
        if (sett.mqtt_on)
        {
            if (name == FPSTR(PARAM_MQTT_HOST))
            {
                save_param(p, sett.mqtt_host, HOST_LEN, errorsObj);
            }
            else if (name == FPSTR(PARAM_MQTT_PORT))
            {
                save_param(p, sett.mqtt_port, errorsObj);
            }
            else if (name == FPSTR(PARAM_MQTT_LOGIN))
            {
                save_param(p, sett.mqtt_login, MQTT_LOGIN_LEN, errorsObj, false);
            }
            else if (name == FPSTR(PARAM_MQTT_PASSWORD))
            {
                save_param(p, sett.mqtt_password, MQTT_PASSWORD_LEN, errorsObj, false);
            }
            else if (name == FPSTR(PARAM_MQTT_TOPIC))
            {
                save_param(p, sett.mqtt_topic, MQTT_TOPIC_LEN, errorsObj, false);
            }

            if (sett.mqtt_auto_discovery)
            {
                if (name == FPSTR(PARAM_MQTT_DISCOVERY_TOPIC))
                {
                    save_param(p, sett.mqtt_discovery_topic, MQTT_TOPIC_LEN, errorsObj, false);
                }
            }
        }
        
        if (sett.dhcp_off)
        {
            if (name == FPSTR(PARAM_IP))
            {
                save_ip_param(p, sett.ip, errorsObj);
            }
            else if (name == FPSTR(PARAM_GATEWAY))
            {
                save_ip_param(p, sett.gateway, errorsObj);
            }
            else if (name == FPSTR(PARAM_MASK))
            {
                save_ip_param(p, sett.mask, errorsObj);
            }
        }
        
        if (name == FPSTR(PARAM_WAKEUP_PER_MIN))
        {
            save_param(p, sett.wakeup_per_min, errorsObj);
            sett.set_wakeup = sett.wakeup_per_min;
        }
        else if (name == FPSTR(PARAM_NTP_SERVER))
        {
            save_param(p, sett.ntp_server, HOST_LEN, errorsObj);
        }
        else if (name == FPSTR(PARAM_SSID))
        {
            save_param(p, sett.wifi_ssid, WIFI_SSID_LEN, errorsObj);
        }
        else if (name == FPSTR(PARAM_PASSWORD))
        {
            save_param(p, sett.wifi_password, WIFI_PWD_LEN, errorsObj, false);
        }

        else if (name == FPSTR(PARAM_WIFI_PHY_MODE))
        {
            save_param(p, sett.wifi_phy_mode, errorsObj, true);
        }
    }

    store_config(sett);
}

void post_api_save(AsyncWebServerRequest *request)
{
    LOG_INFO(F("POST ") << request->url());
    DynamicJsonDocument json_doc(JSON_DYNAMIC_MSG_BUFFER);
    JsonObject ret = json_doc.to<JsonObject>();
    JsonObject errorsObj = ret.createNestedObject("errors");

    applySettings(request, errorsObj);

    uint8_t input = get_param_uint8(request, FPSTR(PARAM_INPUT));
    applyInputSettings(request, errorsObj, input);

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(json_doc, *response);
    request->send(response);
}

void post_api_save_input_type(AsyncWebServerRequest *request)
{
    LOG_INFO(F("POST ") << request->url());
    DynamicJsonDocument json_doc(JSON_DYNAMIC_MSG_BUFFER);
    JsonObject ret = json_doc.to<JsonObject>();
    JsonObject errorsObj = ret.createNestedObject("errors");

    uint8_t input = get_param_uint8(request, FPSTR(PARAM_INPUT));
    //applySettings(request, errorsObj); ? нужно ли тут
    applyInputSettings(request, errorsObj, input);

    if (input == 0)
    {   
        if (runtime_data.counter_type0 == CounterType::HALL)
        {
            ret[F("redirect")] = F("/input/0/hall_detect.html");
        }
        else if (runtime_data.counter_type0 == CounterType::NONE)
        {
            ret[F("redirect")] = F("/index.html");
        }
        else 
        {
            ret[F("redirect")] = F("/input/0/detect.html");
        }
    } 
    else if (input == 1)
    {
        if (runtime_data.counter_type1 == CounterType::HALL)
        {
            ret[F("redirect")] = F("/input/1/hall_detect.html");
        }
        else if (runtime_data.counter_type1 == CounterType::NONE)
        {
            ret[F("redirect")] = F("/index.html");
        }
        else 
        {
            ret[F("redirect")] = F("/input/1/detect.html");
        }
    }

    bool wizard = find_wizard_param(request);
    if (wizard)
    {
        String redirect = ret[F("redirect")];
        ret[F("redirect")] = redirect + String(F("?wizard=true"));
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(json_doc, *response);
    request->send(response);
}

void get_api_turnoff(AsyncWebServerRequest *request)
{
    LOG_INFO(F("GET ") << request->url());
    exit_portal_flag = true;
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "");
    request->send(response);
}

void post_api_reset(AsyncWebServerRequest *request)
{
    LOG_INFO(F("POST ") << request->url());

    DynamicJsonDocument json_doc(JSON_SMALL_STATIC_MSG_BUFFER);
    JsonObject ret = json_doc.to<JsonObject>();

    ret[F("redirect")] = F("/");

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(json_doc, *response);

    factory_reset_flag = true;

    request->send(response);
}