#include "active_point_api.h"
#include "active_point.h"
#include <IPAddress.h>
#include <LittleFS.h>

#include "setup.h"
#include "Logging.h"
#include "master_i2c.h"
#include "utils.h"
#include "config.h"
#include "core/readings.h"
#include "core/input.h"
#include "core/diagnostics.h"
#include "core/alarm.h"
#include "core/url.h"
#include "core/wifi.h"
#include "wifi_helpers.h"
#include "resources.h"
#include "ha/resources.h"

extern bool exit_portal_flag;
extern bool start_connect_flag;
extern wl_status_t wifi_connect_status;
extern bool factory_reset_flag;
extern bool esp_restarted_flag;
extern void send_alarm_config(const Settings &sett);   // main.cpp

extern AttinyData data;
extern AttinyData runtime_data;
extern MasterI2C masterI2C;
extern Settings sett;
extern CalculatedData cdata;

static JsonDocument g_json_doc;

inline void send_json_response(AsyncWebServerRequest *request, JsonDocument &json_doc)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (response)
    {
        serializeJson(json_doc, *response);
        request->send(response);
    }
    else
    {
        request->send(503);
    }
}

// get_auto_factor() — в core/readings.h

/**
 * @brief Запрос состояния подключения к роутеру.
 *        После успеха или не успеха - переадресация на другую страницу.
 *
 * @param request запрос
 */
void get_api_connect_status(AsyncWebServerRequest *request)
{
    LOG_INFO(F("GET ") << request->url());

    g_json_doc.clear();
    JsonObject ret = g_json_doc.to<JsonObject>();

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

    send_json_response(request, g_json_doc);
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
        g_json_doc.clear();
        JsonArray array = g_json_doc.to<JsonArray>();

        for (int i = 0; i < n; ++i)
        {
            LOG_INFO(WiFi.SSID(i) << " " << WiFi.RSSI(i));
            JsonObject obj = array.add<JsonObject>();
            obj["ssid"] = WiFi.SSID(i);
            obj["level"] = int(round(map(WiFi.RSSI(i), -100, -50, 1, 4)));

            // Канал и BSSID именно этой сети, а не текущего подключения:
            // портал вернёт их в форме, и первый коннект пойдёт без скана
            obj["wifi_channel"] = WiFi.channel(i);
            obj["bssid"] = WiFi.BSSIDstr(i);
        }

        write_ssid_to_file();

        WiFi.scanDelete();

        send_json_response(request, g_json_doc);
    }
};

/**
 * @brief Канал и BSSID выбранной сети из скрытых полей формы.
 *
 * Их отдаёт скан вместе со списком сетей (get_api_networks). Зная пару, ЕСП
 * подключается без полного скана эфира — это секунды работы радио, главная
 * статья расхода батареи в сеансе.
 *
 * Вызывается после applySettings, а не из его цепочки: сохранение SSID и
 * пароля сбрасывает кэш коннекта, и при разборе в общем порядке результат
 * зависел бы от порядка полей в форме.
 *
 * Пара пишется целиком: канал без BSSID означал бы подключение к точке
 * 00:00:00:00:00:00 (см. core/wifi.h:has_bssid). Ничего не пришло или не
 * разобралось — оставляем нули, то есть полный скан.
 *
 * @return true если пара сохранена — вызывающему нужно дописать конфигурацию:
 *         applySettings свой store_config сделал раньше.
 */
bool save_fast_connect(AsyncWebServerRequest *request)
{
    const AsyncWebParameter *channel_param = request->getParam(FPSTR(PARAM_WIFI_CHANNEL), true);
    const AsyncWebParameter *bssid_param = request->getParam(FPSTR(PARAM_BSSID), true);

    if (!channel_param || !bssid_param)
        return false;

    const uint8_t channel = parse_wifi_channel(channel_param->value().c_str());

    uint8_t bssid[6] = {0};
    if (!channel || !parse_bssid(bssid_param->value().c_str(), bssid) || !has_bssid(bssid))
        return false;

    sett.wifi_channel = channel;
    memcpy(sett.wifi_bssid, bssid, sizeof(sett.wifi_bssid));

    LOG_INFO(F("Fast connect: channel=") << sett.wifi_channel << F(" bssid=") << bssid_param->value());
    return true;
}

/**
 * @brief Подключение к точки доступа
 *
 * @param request запрос
 */
void post_api_save_connect(AsyncWebServerRequest *request)
{
    feed_portal_watchdog();   // продлеваем режим настройки (#305)
    LOG_INFO(F("POST ") << request->url());

    g_json_doc.clear();
    JsonObject ret = g_json_doc.to<JsonObject>();
    JsonObject errorsObj = ret[F("errors")].to<JsonObject>();

    // Если канал WiFi отличен от текущего канала AP ESP, то возможно отключение телефона
    uint8_t channel = sett.wifi_channel;

    applySettings(request, errorsObj);

    bool wizard = find_wizard_param(request);

    if (!errorsObj.size())
    {
        ret.remove(F("errors"));

        if (save_fast_connect(request))
        {
            store_config(sett);
        }

        bool channel_changed = (channel != sett.wifi_channel);

        if (channel_changed && wizard)
        {
            ret[F("redirect")] = F("/api/start_connect?wizard=true&error=0");
        }
        else if (channel_changed)
        {
            ret[F("redirect")] = F("/api/start_connect?error=0");
        }
        else if (wizard)
        {
            ret[F("redirect")] = F("/api/start_connect?wizard=true");
        }
        else
        {
            ret[F("redirect")] = F("/api/start_connect");
        }
    }

    send_json_response(request, g_json_doc);
}

/**
 * @brief Подключение к точки доступа
 *
 * @param request запрос
 */
void get_api_start_connect(AsyncWebServerRequest *request)
{
    feed_portal_watchdog();   // продлеваем режим настройки (#305)
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
 * @brief Плашка про конкретный вход.
 *
 * Номер входа уезжает отдельным полем: текст один на оба входа, название
 * входа подставит портал (tr() в data/static/strings.js). Иначе на каждое
 * сообщение пришлось бы заводить по две строки-близнеца.
 *
 * @param error код строки сообщения
 * @param input номер входа, INPUT0_RED или INPUT1_BLUE
 * @param page страница настройки, на которую ведёт ссылка
 */
static void add_input_problem(JsonArray &array, const __FlashStringHelper *error,
                              const uint8_t input, const char *page)
{
    JsonObject obj = array.add<JsonObject>();
    obj["error"] = error;
    obj["input"] = input;
    obj["link_text"] = F("5"); // S_SETUP Настроить

    char link_buf[32];
    snprintf(link_buf, sizeof(link_buf), "/input/%u/%s.html", input, page);
    obj["link"] = link_buf;
}

/**
 * @brief Список диагностических сообщений на Главной странице вебсервера
 *
 * @param request запрос
 */
void get_api_main_status(AsyncWebServerRequest *request)
{
    LOG_INFO(F("GET ") << request->url());

    g_json_doc.clear();
    JsonArray array = g_json_doc.to<JsonArray>();

    /*
    Перезагрузка ЕСП во время настройки (#354). Пользователь видит, что
    портал начался заново, и не понимает, сохранились ли настройки. Признак
    посчитан на старте: сейчас флаг attiny взведён в любом случае.
    */
    if (esp_restarted_flag)
    {
        JsonObject obj = array.add<JsonObject>();
        obj["error"] = F("22"); // S_ESP_RESTARTED "Ватериус внештатно перезагрузился..."
    }

    /*
    Ошибки настройки счётчиков (#283). Считаем на каждый запрос, а не один
    раз на старте: когда пользователь заново вводит показания, стартовые
    импульсы уезжают на текущие, накопленный расход обнуляется и плашка
    гаснет сама.
    */
    apply_counter_types(data, runtime_data);   // тип входа мог смениться в этом сеансе (#360)
    const SetupProblems problems = check_setup(sett, data);

    if (problems.factor_too_big0)
    {   // S_FACTOR_TOO_BIG "Счётчик %s насчитал в разы больше второго..."
        add_input_problem(array, F("23"), INPUT0_RED, "settings");
    }
    if (problems.factor_too_big1)
    {
        add_input_problem(array, F("23"), INPUT1_BLUE, "settings");
    }
    if (problems.silent_input0)
    {   // S_INPUT_SILENT "Счётчик %s не насчитал ни одного импульса..."
        add_input_problem(array, F("24"), INPUT0_RED, "setup");
    }
    if (problems.silent_input1)
    {
        add_input_problem(array, F("24"), INPUT1_BLUE, "setup");
    }

    wl_status_t status = WiFi.status();
    LOG_INFO(F("WIFI: status=") << status);

    if (status == WL_CONNECT_FAILED || status == WL_CONNECTION_LOST || status == WL_WRONG_PASSWORD)
    {
        JsonObject obj = array.add<JsonObject>();
        obj["error"] = F("1");  // S_WIFI_CONNECT "Ошибка подключения к Wi-Fi"
        obj["link_text"] = F("5"); // S_SETUP Настроить
        char link_buf[48];
        snprintf(link_buf, sizeof(link_buf), "/wifi_settings.html?status_code=%d", status);
        obj["link"] = link_buf;
    }
    else
    {
        if (sett.factor1 == AUTO_IMPULSE_FACTOR)
        {
            if (status == WL_CONNECTED)
            {
                JsonObject obj = array.add<JsonObject>();
                obj["error"] = F("2");  // S_SETUP_COUNTERS "Ватериус успешно подключился к Wi-Fi. Теперь настроим счётчики."
                obj["link_text"] = F("5"); // S_SETUP Настроить
                obj["link"] = F("/input/1/setup.html");
            }
            else
            {
                JsonObject obj = array.add<JsonObject>();
                obj["error"] = F("3");  // S_NEED_SETUP "Ватериус ещё не настроен"
                obj["link_text"] = F("6"); // S_LETSGO Приступить
                obj["link"] = F("/captive_portal_start.html");
            }
        }
    }

    LOG_INFO(F("JSON: Size: ") << measureJson(g_json_doc));

    send_json_response(request, g_json_doc);
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

    g_json_doc.clear();
    JsonObject ret = g_json_doc.to<JsonObject>();

    if (masterI2C.getAttinyData(runtime_data))
    {
        const uint16_t factor_cold = get_auto_factor(runtime_data.impulses1, data.impulses1, sett.factor1, sett.factor1);

        if (index == INPUT0_RED)
        {
            ret[F("state")] = int(runtime_data.impulses0 > data.impulses0);
            ret[F("factor")] = get_auto_factor(runtime_data.impulses0, data.impulses0, sett.factor0, factor_cold);
            ret[F("impulses")] = runtime_data.impulses0 - data.impulses0;
        }
        else if (index == INPUT1_BLUE)
        {
            ret[F("state")] = int(runtime_data.impulses1 > data.impulses1);
            ret[F("factor")] = factor_cold;
            ret[F("impulses")] = runtime_data.impulses1 - data.impulses1;
        }
        // root[F("elapsed")] = (uint32_t)(SETUP_TIME_SEC - millis() / 1000.0);
    }
    else
    {
        ret[F("error")] = F("7"); // S_NO_LINK Ошибка связи с МК
    }

#if WATERIUS_MODEL == WATERIUS_MODEL_2
    digitalWrite(CH0_LED_PIN, runtime_data.on_pulse0);
    digitalWrite(CH1_LED_PIN, runtime_data.on_pulse1);
#endif

    send_json_response(request, g_json_doc);
};

// is_all_asterisks(), strncpy_trimmed() и правила проверки значений — в core/input.h

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

/**
 * @brief Записывает код ошибки от ядра в JSON ответа и в лог
 */
static void report_param_error(const AsyncWebParameter *p, JsonObject &errorsObj, ParamError err)
{
    switch (err)
    {
    case PARAM_ERR_LENGTH:
        LOG_ERROR(FPSTR(ERROR_LENGTH_ERROR) << ": " << p->name());
        errorsObj[p->name()] = String(F("14"));  // Превышена длина поля
        break;
    case PARAM_ERR_EMPTY:
        LOG_ERROR(FPSTR(ERROR_EMPTY) << ": " << p->name());
        errorsObj[p->name()] = String(F("17"));  // Значение не может быть пустым
        break;
    case PARAM_ERR_VALUE:
        LOG_ERROR(FPSTR(ERROR_VALUE) << ": " << p->name());
        errorsObj[p->name()] = String(F("15"));  // Неверное значение
        break;
    case PARAM_ERR_NO_COMMA:
        LOG_ERROR(FPSTR(ERROR_NO_COMMA) << ": " << p->name());
        errorsObj[p->name()] = String(F("19"));  // Похоже, забыта запятая
        break;
    case PARAM_ERR_TLS:
        LOG_ERROR(FPSTR(ERROR_TLS) << ": " << p->name());
        errorsObj[p->name()] = String(F("20"));  // Шифрование не поддерживается
        break;
    case PARAM_ERR_PORT_IN_HOST:
        LOG_ERROR(FPSTR(ERROR_PORT_IN_HOST) << ": " << p->name());
        errorsObj[p->name()] = String(F("21"));  // Порт указывайте в отдельном поле
        break;
    case PARAM_OK:
    case PARAM_MASKED:
        break;   // не ошибки
    }
    // без default: новый код ошибки не должен молча исчезнуть — компилятор
    // напомнит про незакрытую ветку
}

void save_param(const AsyncWebParameter *p, char *dest, size_t size, JsonObject &errorsObj, bool required /*true*/)
{
    ParamError err = parse_text(dest, size, p->value().c_str(), required);

    if (err == PARAM_MASKED)
    {
        LOG_INFO(F("NOT ") << FPSTR(PARAM_SAVED) << p->name() << F(" **** value"));
    }
    else if (err == PARAM_OK)
    {
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << dest);
    }
    else
    {
        report_param_error(p, errorsObj, err);
    }
}

void save_param(const AsyncWebParameter *p, uint16_t &v, JsonObject &errorsObj, const bool zero_ok)
{
    ParamError err = parse_uint16(p->value().c_str(), v, zero_ok);

    if (err == PARAM_OK)
    {
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    }
    else
    {
        report_param_error(p, errorsObj, err);
    }
}

void save_param(const AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj, const bool zero_ok)
{
    ParamError err = parse_uint8(p->value().c_str(), v, zero_ok);

    if (err == PARAM_OK)
    {
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    }
    else
    {
        report_param_error(p, errorsObj, err);
    }
}

void save_bool_param(const AsyncWebParameter *p, uint8_t &v, JsonObject &errorsObj)
{
    ParamError err = parse_bool(p->value().c_str(), v);

    if (err == PARAM_OK)
    {
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    }
    else
    {
        report_param_error(p, errorsObj, err);
    }
}

/**
 * @brief Адрес MQTT брокера. Снимает схему и путь, отвергает шифрование и порт.
 *
 * Отдельная функция, а не перегрузка save_param: от текстовой она отличалась бы
 * только флагом, а рядом уже есть bool required — вызов стал бы нечитаемым.
 */
void save_broker_host(const AsyncWebParameter *p, char *dest, size_t size, JsonObject &errorsObj)
{
    ParamError err = parse_broker_host(dest, size, p->value().c_str());

    if (err == PARAM_OK)
    {
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << dest);
    }
    else
    {
        report_param_error(p, errorsObj, err);
    }
}

/**
 * @brief Показания счётчика. Показания воды обязаны содержать дробную часть.
 *
 * Значение записывается только при успехе: если пользователь забыл запятую,
 * нельзя ни сохранить показания, ни сбросить стартовые импульсы — иначе
 * показания уедут даже после ввода правильного числа. Поэтому, в отличие от
 * остальных save_param, эта возвращает результат.
 *
 * @param counter_name тип счётчика: правило действует только для воды
 * @return true если значение сохранено
 */
bool save_param(const AsyncWebParameter *p, float &v, JsonObject &errorsObj, const uint8_t counter_name)
{
    ParamError err = check_reading(p->value().c_str(), counter_name);
    if (err != PARAM_OK)
    {
        report_param_error(p, errorsObj, err);
        return false;
    }

    float value = 0.0;
    parse_decimal(p->value().c_str(), value);

    v = value;
    LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << v);
    return true;
}

void save_ip_param(const AsyncWebParameter *p, uint32_t &v, JsonObject &errorsObj)
{
    IPAddress ip;
    if (ip.fromString(p->value()))
    {
        v = ip.v4();
        LOG_INFO(FPSTR(PARAM_SAVED) << p->name() << F("=") << ip.toString());
    }
    else
    {
        LOG_ERROR(FPSTR(ERROR_VALUE) << ": " << p->name());
        errorsObj[p->name()] = String(F("15"));  // Неверное значение
    }
}

bool find_wizard_param(AsyncWebServerRequest *request)
{
    for (size_t i = 0; i < request->params(); i++)
    {
        const AsyncWebParameter *p = request->getParam(i);
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
        const AsyncWebParameter *p = request->getParam(i);
        if (p->name() == name)
        {
            return p->value().toInt();
        }
    }
    return 0xFF;
}

void applyInputParameter(const AsyncWebParameter *p, JsonObject &errorsObj, const uint8_t input)
{
    const String &name = p->name();

    LOG_INFO(F("parameter ") << name << "=" << p->value());
    if (name == FPSTR(PARAM_CHANNEL_START) || name == FPSTR(s_ch)) // portal || ha
    {
        switch (input)
        {
            case INPUT0_RED:
                if (save_param(p, sett.channel0_start, errorsObj, sett.counter0_name))
                {
                    sett.impulses0_start = runtime_data.impulses0;
                    sett.impulses0_previous = sett.impulses0_start;
                    LOG_INFO("impulses0_start=" << sett.impulses0_start);
                }
                break;
            case INPUT1_BLUE:
                if (save_param(p, sett.channel1_start, errorsObj, sett.counter1_name))
                {
                    sett.impulses1_start = runtime_data.impulses1;
                    sett.impulses1_previous = sett.impulses1_start;
                    LOG_INFO("impulses1_start=" << sett.impulses1_start);
                }
                break;
        }
    }
    else if (name == FPSTR(PARAM_SERIAL))
    {
        switch (input)
        {
            case INPUT0_RED:
                save_param(p, sett.serial0, SERIAL_LEN, errorsObj, false);
                break;
            case INPUT1_BLUE:
                save_param(p, sett.serial1, SERIAL_LEN, errorsObj, false);
                break;
        }
    }
    else if (name == FPSTR(PARAM_COUNTER_NAME) || name == FPSTR(s_cname)) // portal || ha
    {
        switch(input)
        {
            case INPUT0_RED:
                save_param(p, sett.counter0_name, errorsObj, true);
                break;
            case INPUT1_BLUE:
                save_param(p, sett.counter1_name, errorsObj, true);
                break;
        }

    }
    else if (name == FPSTR(PARAM_COUNTER_TYPE) || name == FPSTR(s_ctype))  // portal || ha
    {
        switch (input)
        {
            case INPUT0_RED:
                if (!masterI2C.setCountersType(p->value().toInt(), runtime_data.counter_type1))
                {
                    LOG_ERROR(FPSTR(ERROR_ATTINY_ERROR) << ": " << p->name());
                    errorsObj[p->name()] = String(F("16")); // Ошибка связи с attiny
                }
                else
                {
                    runtime_data.counter_type0 = p->value().toInt();
                    LOG_INFO(FPSTR(PARAM_SAVED0) << p->name() << F("=") << p->value());
                }
                break;
            case INPUT1_BLUE:
                if (!masterI2C.setCountersType(runtime_data.counter_type0, p->value().toInt()))
                {
                    LOG_ERROR(FPSTR(ERROR_ATTINY_ERROR) << ": " << p->name());
                    errorsObj[p->name()] = String(F("16")); // Ошибка связи с attiny
                }
                else
                {
                    runtime_data.counter_type1 = p->value().toInt();
                    LOG_INFO(FPSTR(PARAM_SAVED1) << p->name() << F("=") << p->value());
                }
                break;
        }

    }
    else if (name == FPSTR(PARAM_ALARM_FLOW) || name == FPSTR(s_af)) // portal || ha
    {
        // Порог расхода: л/ч для объёма, Вт для электричества. 0 - выключено
        switch (input)
        {
            case INPUT0_RED:
                save_param(p, sett.alarm_flow0, errorsObj, true);
                break;
            case INPUT1_BLUE:
                save_param(p, sett.alarm_flow1, errorsObj, true);
                break;
        }
    }
    else if (name == FPSTR(PARAM_ALARM_LEAK) || name == FPSTR(s_al)) // portal || ha
    {
        // Минут непрерывного расхода. 0 - выключено
        switch (input)
        {
            case INPUT0_RED:
                save_param(p, sett.alarm_leak0, errorsObj, true);
                break;
            case INPUT1_BLUE:
                save_param(p, sett.alarm_leak1, errorsObj, true);
                break;
        }
    }
    else if (name == FPSTR(PARAM_FACTOR) || name == FPSTR(s_f)) // portal || ha
    {
        uint16_t value = p->value().toInt();

        // Авто или Как у холодной воды
        if (value == AUTO_IMPULSE_FACTOR || value == AS_COLD_CHANNEL)
        {
            const uint16_t factor_cold = get_auto_factor(runtime_data.impulses1, data.impulses1, sett.factor1, sett.factor1);

            switch (input)
            {
                case INPUT0_RED:
                    sett.factor0 = get_auto_factor(runtime_data.impulses0, data.impulses0, sett.factor0, factor_cold);
                    LOG_INFO(FPSTR(PARAM_FACTOR) << p->name() << F("->") << sett.factor0);
                    break;
                case INPUT1_BLUE:
                    sett.factor1 = factor_cold;
                    LOG_INFO(FPSTR(PARAM_FACTOR) << p->name() << F("->") << sett.factor1);
                    break;
            }
        }
        else
        {
            switch (input)
            {
                case INPUT0_RED:
                    save_param(p, sett.factor0, errorsObj);
                    break;
                case INPUT1_BLUE:
                    save_param(p, sett.factor1, errorsObj);
                    break;
            }
        }
    }
}

void applyInputSettings(AsyncWebServerRequest *request, JsonObject &errorsObj, const uint8_t input)
{
    const int params = request->params();

    LOG_INFO(F("Apply Input ") << params << " parameters");

    for (int i = 0; i < params; i++)
    {
        const AsyncWebParameter *p = request->getParam(i);
        applyInputParameter(p, errorsObj, input);
    }

    store_config(sett);
}

void applyCheckBoxParameter(const AsyncWebParameter *p, JsonObject &errorsObj)
{
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
    else if (name == FPSTR(PARAM_MQTT_RETAIN))
    {
        save_bool_param(p, sett.mqtt_retain, errorsObj);
    }
    else if (name == FPSTR(PARAM_SEND_ON_CONSUMPTION) || name == FPSTR(s_sc))  // portal || ha
    {
        save_bool_param(p, sett.send_on_consumption, errorsObj);
    }
}

void applyNonCheckBoxParameter(const AsyncWebParameter *p, JsonObject &errorsObj)
{
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
            save_broker_host(p, sett.mqtt_host, HOST_LEN, errorsObj);
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

    if (name == FPSTR(s_period_min))
    {
        save_param(p, sett.wakeup_per_min, errorsObj);
        reset_period_min_tuned(sett);
    }
    else if (name == FPSTR(s_voltage_cal))
    {
        save_param(p, sett.voltage_cal, errorsObj);
    }
    else if (name == FPSTR(PARAM_NTP_SERVER))
    {
        save_param(p, sett.ntp_server, HOST_LEN, errorsObj);
    }
    else if (name == FPSTR(PARAM_SSID))
    {
        save_param(p, sett.wifi_ssid, WIFI_SSID_LEN, errorsObj);
        // Сброс кэша быстрого коннекта: канал/BSSID от прошлого роутера
        // становятся неактуальны при смене сети — иначе первый коннект уходит
        // на старый канал (в AP_STA портале это ещё и роняет телефон).
        sett.wifi_channel = 0;
        memset(sett.wifi_bssid, 0, sizeof(sett.wifi_bssid));
    }
    else if (name == FPSTR(PARAM_PASSWORD))
    {
        save_param(p, sett.wifi_password, WIFI_PWD_LEN, errorsObj, false);
        // См. комментарий выше: смена пароля тоже сбрасывает кэш коннекта.
        sett.wifi_channel = 0;
        memset(sett.wifi_bssid, 0, sizeof(sett.wifi_bssid));
    }

    else if (name == FPSTR(PARAM_WIFI_PHY_MODE))
    {
        save_param(p, sett.wifi_phy_mode, errorsObj, true);
    }
    else if (name == FPSTR(PARAM_COMPANY))
    {
        save_param(p, sett.company, COMPANY_LEN, errorsObj, false);
    }
    else if (name == FPSTR(PARAM_PLACE))
    {
        save_param(p, sett.place, PLACE_LEN, errorsObj, false);
    }
}

void applySettings(AsyncWebServerRequest *request, JsonObject &errorsObj)
{
    const int params = request->params();

    LOG_INFO(F("Apply ") << params << " parameters");

    // Вначале bool, чтобы дальше проверять только требуемые параметры
    for (int i = 0; i < params; i++)
    {
        const AsyncWebParameter *p = request->getParam(i);
        applyCheckBoxParameter(p, errorsObj);
    }

    for (int i = 0; i < params; i++)
    {
        const AsyncWebParameter *p = request->getParam(i);
        applyNonCheckBoxParameter(p, errorsObj);
    }

    store_config(sett);
}

/*
Необязательное uint16-поле формы: чего нет в запросе, того не трогаем.
*/
static void save_uint16_param(AsyncWebServerRequest *request, const String &name,
                              uint16_t &value, JsonObject &errorsObj)
{
    if (!request->hasParam(name, true))
        return;

    const AsyncWebParameter *p = request->getParam(name, true);
    save_param(p, value, errorsObj, true);  // ноль допустим: тревога выключена
}

void post_api_save(AsyncWebServerRequest *request)
{
    feed_portal_watchdog();   // продлеваем режим настройки (#305)
    LOG_INFO(F("POST ") << request->url());
    g_json_doc.clear();
    JsonObject ret = g_json_doc.to<JsonObject>();
    JsonObject errorsObj = ret[F("errors")].to<JsonObject>();

    applySettings(request, errorsObj);

    uint8_t input = get_param_uint8(request, FPSTR(PARAM_INPUT));
    applyInputSettings(request, errorsObj, input);

    send_json_response(request, g_json_doc);
}

/*
Пороги тревог (#202).

Отдельный обработчик, а не /api/save: та страница настраивает один вход, а
тревоги показываются обоими каналами сразу, чтобы не гонять пользователя по
двум формам ради двух чисел.

Пороги тут же уезжают в attiny: у неё они живут в ОЗУ, и ждать следующего
сеанса значило бы, что настройка вступит в силу через сутки.
*/
void post_api_save_alarms(AsyncWebServerRequest *request)
{
    feed_portal_watchdog();   // продлеваем режим настройки (#305)
    LOG_INFO(F("POST ") << request->url());
    g_json_doc.clear();
    JsonObject ret = g_json_doc.to<JsonObject>();
    JsonObject errorsObj = ret[F("errors")].to<JsonObject>();

    save_uint16_param(request, FPSTR(PARAM_ALARM_FLOW0), sett.alarm_flow0, errorsObj);
    save_uint16_param(request, FPSTR(PARAM_ALARM_LEAK0), sett.alarm_leak0, errorsObj);
    save_uint16_param(request, FPSTR(PARAM_ALARM_FLOW1), sett.alarm_flow1, errorsObj);
    save_uint16_param(request, FPSTR(PARAM_ALARM_LEAK1), sett.alarm_leak1, errorsObj);

    if (errorsObj.size() == 0)
    {
        store_config(sett);
        send_alarm_config(sett);
        ret[F("redirect")] = F("/index.html");
    }

    send_json_response(request, g_json_doc);
}

void post_api_save_input_type(AsyncWebServerRequest *request)
{
    feed_portal_watchdog();   // продлеваем режим настройки (#305)
    LOG_INFO(F("POST ") << request->url());
    g_json_doc.clear();
    JsonObject ret = g_json_doc.to<JsonObject>();
    JsonObject errorsObj = ret[F("errors")].to<JsonObject>();

    uint8_t input = get_param_uint8(request, FPSTR(PARAM_INPUT));
    //applySettings(request, errorsObj); ? нужно ли тут
    applyInputSettings(request, errorsObj, input);

    if (input == INPUT0_RED)
    {
        // Датчик протечки - не счётчик: ни веса импульса, ни показаний (#202)
        if (runtime_data.counter_type0 == CounterType::LEAKAGE)
        {
            ret[F("redirect")] = F("/index.html");
        }
        else if (sett.counter0_name == CounterName::ELECTRO)
        {
            ret[F("redirect")] = F("/input/0/input_electro_detect.html");
        }
        else if (runtime_data.counter_type0 == CounterType::NONE)
        {
            ret[F("redirect")] = F("/index.html");
        }
        else if (sett.factor0 == AS_COLD_CHANNEL) // Первая настройка
        {
            ret[F("redirect")] = F("/input/0/detect.html");
        }
        else
        {
            ret[F("redirect")] = F("/input/0/settings.html");
        }
    }
    else if (input == INPUT1_BLUE)
    {
        // Датчик протечки - не счётчик: ни веса импульса, ни показаний (#202)
        if (runtime_data.counter_type1 == CounterType::LEAKAGE)
        {
            ret[F("redirect")] = F("/index.html");
        }
        else if (sett.counter1_name == CounterName::ELECTRO)
        {
            ret[F("redirect")] = F("/input/1/input_electro_detect.html");
        }
        else if (runtime_data.counter_type1 == CounterType::NONE)
        {
            ret[F("redirect")] = F("/index.html");
        }
        else if (sett.factor1 == AUTO_IMPULSE_FACTOR) // Первая настройка
        {
            ret[F("redirect")] = F("/input/1/detect.html");
        }
        else
        {
            ret[F("redirect")] = F("/input/1/settings.html");
        }
    }

    bool wizard = find_wizard_param(request);
    if (wizard)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s?wizard=true", ret[F("redirect")].as<const char*>());
        ret[F("redirect")] = buf;
    }

    send_json_response(request, g_json_doc);
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
    feed_portal_watchdog();   // продлеваем режим настройки (#305)
    LOG_INFO(F("POST ") << request->url());

    g_json_doc.clear();
    JsonObject ret = g_json_doc.to<JsonObject>();

    ret[F("redirect")] = F("/");

    factory_reset_flag = true;

    send_json_response(request, g_json_doc);
}