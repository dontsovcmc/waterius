#include "UserClass.h"
#include "WateriusHttps.h"
#include "Logging.h"
#include <ArduinoJson.h>


#define JSON_BUFFER_SIZE 1000


bool UserClass::sendNewData(const Settings &settings, const SlaveData &data, const float &channel0, const float &channel1)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send new data";
    constexpr char THIS_FUNC_SVC[] = "SND";
    LOG_NOTICE(THIS_FUNC_SVC, "-- START -- " << THIS_FUNC_DESCRIPTION);
    
    if (strnlen(settings.waterius_key, WATERIUS_KEY_LEN) == 0) {
        LOG_NOTICE(THIS_FUNC_SVC, "NO Waterius key. SKIP");
        return false;
    };

    // Set JSON body
    String jsonBody;
    StaticJsonDocument<JSON_BUFFER_SIZE> root;
    root["delta0"] =        (data.impulses0 - settings.impulses0_previous)*settings.liters_per_impuls;
    root["delta1"] =        (data.impulses1 - settings.impulses1_previous)*settings.liters_per_impuls;
    root["good"] =          data.diagnostic;
    root["boot"] =          data.version;
    root["ch0"] =           channel0;
    root["ch1"] =           channel1;
    root["imp0"] =          data.impulses0;
    root["imp1"] =          data.impulses1;
    root["version"] =       settings.version;
    root["voltage"] =       (float)(data.voltage/1000.0);
    root["version_esp"] =   FIRMWARE_VERSION;
    root["key"] =           settings.waterius_key;
    root["resets"] =        data.resets;
    root["email"] =         settings.waterius_email;
    serializeJson(root, jsonBody);

    // Try to send
    WateriusHttps::ResponseData responseData = WateriusHttps::sendJsonPostRequest(
        settings.waterius_host, settings.waterius_key, settings.waterius_email, jsonBody);
    bool send_result = responseData.isSuccess && responseData.code == 200;

    LOG_INFO(THIS_FUNC_SVC, "Send HTTP code:\t" << responseData.code);
    LOG_INFO(THIS_FUNC_SVC, "Send result:\t" << (send_result ? "Success" : "Error"));
    LOG_NOTICE(THIS_FUNC_SVC, "-- END --");

    return send_result;
}
