#include "blink.h"
#include "types.h"   // WATERIUS_MODEL_2

SendStatus merge_status(const SendStatus a, const SendStatus b)
{
    // Значения перечисления упорядочены от «не настроен» к «нет связи»,
    // поэтому худший — просто больший
    return a > b ? a : b;
}

SendStatus cloud_status(const SessionStatus &status)
{
    return merge_status(status.waterius, status.http);
}

ErrorBlynks blink_code(const SessionStatus &status)
{
    if (!status.config_loaded)
        return ERROR_CONFIG;

    if (!status.wifi_connected)
        return ERROR_CONNECT_ROUTER;

    const SendStatus cloud = cloud_status(status);

    if (cloud == SEND_NO_CONNECTION)
        return ERROR_CONNECT_CLOUD;

    if (cloud == SEND_BAD_ANSWER)
        return ERROR_CLOUD_ANSWER;

    if (status.mqtt == SEND_NO_CONNECTION || status.mqtt == SEND_BAD_ANSWER)
        return ERROR_CONNECT_MQTT;

#if WATERIUS_MODEL == WATERIUS_MODEL_2
    if (status.low_voltage)
        return ERROR_LOW_VOLTAGE;
#endif

    return ERROR_OK;
}
