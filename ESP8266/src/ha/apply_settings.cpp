#include "apply_settings.h"
#include <ESPAsyncWebServer.h>
#include "Logging.h"
#include "portal/active_point_api.h"
#include "config.h"
#include "setup.h"

extern Settings sett;

void apply_settings(const JsonDocument &json_settings_received)
{
    JsonDocument errors_doc;
    JsonObject errorsObj = errors_doc.to<JsonObject>();

    JsonObjectConst root = json_settings_received.as<JsonObjectConst>();
    for (JsonPairConst kv : root)
    {
        String name = kv.key().c_str();
        String payload = kv.value().as<String>();

        LOG_INFO(F("Apply setting: ") << name << F("=") << payload);

        if (name.endsWith(F("0")))
        {
            name.remove(name.length() - 1, 1);
            AsyncWebParameter p(name, payload);
            applyInputParameter(&p, errorsObj, 0);
        }
        else if (name.endsWith(F("1")))
        {
            name.remove(name.length() - 1, 1);
            AsyncWebParameter p(name, payload);
            applyInputParameter(&p, errorsObj, 1);
        }
        else
        {
            AsyncWebParameter p(name, payload);
            applyCheckBoxParameter(&p, errorsObj);
            applyNonCheckBoxParameter(&p, errorsObj);
        }
    }

    store_config(sett);
}