#include "ESPAsyncWebServer.h"
#include "subscribe.h"
#include "Logging.h"
#include "publish.h"
#include "publish_data.h"
#include "config.h"
#include "json.h"
#include "utils.h"
#include "portal/active_point_api.h"


#define MQTT_MAX_TRIES 5
#define MQTT_CONNECT_DELAY 100
#define MQTT_SUBSCRIPTION_TOPIC "/#"

extern MasterI2C masterI2C;
extern AttinyData data;
extern AttinyData runtime_data;
extern CalculatedData cdata;

/*
Команды, которые приняты, но ещё не применены.

Удерживаемая команда - это гарантия доставки: пока она лежит у брокера,
пропущенный сеанс означает отсрочку, а не потерю. Поэтому снимаем retain не при
получении, а после применения (clear_applied_commands): иначе команда,
приехавшая в конце сеанса, исчезала бы у брокера, так и не подействовав.

Восемь - с запасом: столько разных параметров за один сеанс не присылают. А
переполнение безопасно: незапомненная команда останется удерживаемой и приедет
в следующий раз.
*/
#define MAX_PENDING_COMMANDS 8
static String pending_commands[MAX_PENDING_COMMANDS];
static uint8_t pending_count = 0;

/*
Признак «пришла команда». Флаг, а не размер документа: повторная команда тем же
параметром документ не растит - ключ в нём один, - а применить её надо.
*/
static volatile bool command_arrived = false;

/**
 * @brief Забрать признак прихода команды, сбросив его
 */
bool take_command_arrived()
{
    const bool was = command_arrived;
    command_arrived = false;
    return was;
}

static void remember_command(const String &topic)
{
    for (uint8_t i = 0; i < pending_count; i++)
    {
        if (pending_commands[i] == topic)
        {
            return;
        }
    }

    if (pending_count < MAX_PENDING_COMMANDS)
    {
        pending_commands[pending_count++] = topic;
    }
}

/**
 * @brief Забыть у брокера команды, которые уже применены
 *
 * Зовётся после применения и повторной отправки данных, пока сеанс с брокером
 * ещё открыт.
 *
 * @param mqtt_client клиент MQTT
 */
void clear_applied_commands(PubSubClient &mqtt_client)
{
    for (uint8_t i = 0; i < pending_count; i++)
    {
        clear_retained(mqtt_client, pending_commands[i]);
        pending_commands[i] = String();
    }
    pending_count = 0;
}

/**
 * @brief Обновление настроек по сообщению MQTT
 *
 * @param topic топик вида /period_min/set
 * @param payload данные из топика
 * @return true пришла команда,
 * @return false команды не было
 */
bool ha_fill_json_settings_data(const String &topic, const String &payload, JsonDocument &json_settings_received)
{
    // Пустое сообщение в топике команды - снятие retain, а не значение:
    // устройство публикует его само и получает обратно по своей подписке (#421)
    if (payload.length() == 0 || !topic.endsWith(F("/set")))
    {
        return false;
    }

    // извлекаем имя параметра
    int endslash = topic.lastIndexOf('/');
    int prevslash = topic.lastIndexOf('/', endslash - 1);
    String name = topic.substring(prevslash + 1, endslash);
    LOG_INFO(F("MQTT: CALLBACK: Parameter ") << name);

    if (name == F("ota"))
    {
        JsonDocument ota_doc;
        if (deserializeJson(ota_doc, payload) == DeserializationError::Ok)
        {
            json_settings_received[name] = ota_doc.as<JsonObject>();
        }
        else
        {
            LOG_ERROR(F("MQTT: Failed to parse OTA JSON"));
        }
    }
    else
    {
        json_settings_received[name] = payload;
    }

    return true;
}

/**
 * @brief Обработка пришедшего сообщения по подписке
 *
 * @param sett настройки
 * @param json_data данные JSON
 * @param mqtt_client клиент MQTT
 * @param mqtt_topic sett.mqtt_topic
 * @param raw_topic топик
 * @param raw_payload  данные из топика
 * @param length длина сообщения
 */
void mqtt_callback(Settings &sett, JsonDocument &json_settings_received, PubSubClient &mqtt_client, String &mqtt_topic, char *raw_topic, byte *raw_payload, unsigned int length)
{
    String topic = raw_topic;
    String payload;
    payload.reserve(length);

    LOG_INFO(F("MQTT: CALLBACK: Message arrived to: ") << topic);
    LOG_INFO(F("MQTT: CALLBACK: Message length: ") << length);

    for (unsigned int i = 0; i < length; i++)
    {
        payload += (char)raw_payload[i];
    }
    LOG_INFO(F("MQTT: CALLBACK: Message payload: ") << payload);

    // Трогаем только свои команды: подписка накрывает и топики показаний, а
    // стирать их нельзя - из них Home Assistant берёт значения сразу после
    // перезапуска (#422)
    if (!ha_fill_json_settings_data(topic, payload, json_settings_received))
    {
        return;
    }

    /*
    Команду обновления снимаем сразу, не откладывая. Обновление выполняется уже
    после закрытия сеанса с брокером и заканчивается перезагрузкой, так что
    снять её потом будет нечем, а оставленная удерживаемой она запускала бы
    обновление на каждом пробуждении.
    */
    if (topic.endsWith(F("/ota/set")))
    {
        clear_retained(mqtt_client, topic);
        return;
    }

    command_arrived = true;
    remember_command(topic);
}

/**
 * @brief Подключается к серверу MQTT c таймаутом и несколькими попытками
 *
 * @param sett настройки
 * @param mqtt_client клиент MQTT
 * @param mqtt_topic строка с топиком
 * @return true Удалось подключиться,
 * @return false Не удалось подключиться
 */
bool mqtt_connect(Settings &sett, PubSubClient &mqtt_client)
{
    String client_id = get_device_name();
    const char *login = sett.mqtt_login[0] ? sett.mqtt_login : NULL;
    const char *pass = sett.mqtt_password[0] ? sett.mqtt_password : NULL;
    LOG_INFO(F("MQTT: Connecting..."));
    int attempts = MQTT_MAX_TRIES;
    do
    {
        LOG_INFO(F("MQTT: Attempt #") << MQTT_MAX_TRIES - attempts + 1 << F(" from ") << MQTT_MAX_TRIES);
        if (mqtt_client.connect(client_id.c_str(), login, pass))
        {
            LOG_INFO(F("MQTT: Connected."));
            return true;
        }
        LOG_ERROR(F("MQTT: Connect failed with state ") << mqtt_client.state());
        delay(MQTT_CONNECT_DELAY);
    } while (--attempts);

    return false;
}

/**
 * @brief Подписка на все субтопики устройства
 *
 * @param mqtt_client клиент MQTT
 * @param mqtt_topic строка с топиком
 * @return true удалось подключиться,
 * @return false не удалось подключиться
 */
bool mqtt_subscribe(PubSubClient &mqtt_client, String &mqtt_topic)
{
    String subscribe_topic = mqtt_topic + F(MQTT_SUBSCRIPTION_TOPIC);
    if (!mqtt_client.subscribe(subscribe_topic.c_str(), 1))
    {
        LOG_ERROR(F("MQTT: Failed Subscribe to ") << subscribe_topic);
        return false;
    }

    LOG_INFO(F("MQTT: Subscribed to ") << subscribe_topic);

    return true;
}

/**
 * @brief Отписка от сообщений на все субтопики устройства
 *
 * @param mqtt_client
 * @param mqtt_topic
 * @return true
 * @return false
 */
bool mqtt_unsubscribe(PubSubClient &mqtt_client, String &mqtt_topic)
{
    String subscribe_topic = mqtt_topic + F(MQTT_SUBSCRIPTION_TOPIC);
    if (!mqtt_client.unsubscribe(subscribe_topic.c_str()))
    {
        LOG_ERROR(F("MQTT: Failed Unsubscribe from ") << subscribe_topic);
        return false;
    }

    LOG_INFO(F("MQTT: Unsubscribed from ") << subscribe_topic);

    return true;
}