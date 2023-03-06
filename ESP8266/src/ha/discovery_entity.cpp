#include "discovery_entity.h"
#include "Logging.h"
#include "utils.h"
#include "publish.h"
#include "resources.h"

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
