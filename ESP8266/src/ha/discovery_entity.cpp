#include "discovery_entity.h"
#include <ArduinoJson.h>
#include "Logging.h"
#include "utils.h"
#include "publish.h"
#include "resources.h"

/**
 * @brief Формирует шаблон для атрибутов сенсора
 *
 * @param attrs массив атрибутов
 * @param index индекс первого атрибута в масссиве
 * @param count количестов атрибутов в масссиве
 * @param channel канал
 * @return String строка с шаблоном для извлечения атрибутов для сенсора
 */
String get_attributes_template(const char *const attrs[][MQTT_PARAM_COUNT], int index, int count, int channel)
{
    String json_attributes_template = "";

    DynamicJsonDocument json_doc(JSON_DYNAMIC_MSG_BUFFER);
    JsonObject json_attributes = json_doc.to<JsonObject>();
    String attribute_name;
    String attribute_id;
    String attribute_template;

    for (int i = 0; i < count; i++)
    {
        attribute_name = FPSTR(attrs[index + i][1]);
        attribute_id = FPSTR(attrs[index + i][2]);

        update_channel_names(channel, attribute_id, attribute_name);

        attribute_template = String(F("{{ value_json.")) + attribute_id + F(" | is_defined }}");

        json_attributes[attribute_name] = attribute_template;
    }

    serializeJson(json_attributes, json_attributes_template);
    return json_attributes_template;
}

/**
 * @brief Изменяет названия и идентификаторы сенсоров с учетом названия и идентификатора канала
 *
 * @param channel номер канала
 * @param entity_id идентификатор сенсора
 * @param entity_name имя сенсора
 */
void update_channel_names(int channel, String &entity_id, String &entity_name)
{
    if (channel != NONE)
    {
        entity_id += channel;
        entity_name = String(FPSTR(CHANNEL_NAMES[channel])) + " " + entity_name;
    }
}
