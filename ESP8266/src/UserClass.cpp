#include "UserClass.h"
#include "WateriusHttps.h"
#include "Logging.h"
#include <ArduinoJson.h>


#define JSON_BUFFER_SIZE 500


bool UserClass::sendNewData(const Settings &settings, const SlaveData &data, const CalculatedData &cdata)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send new data";
    constexpr char THIS_FUNC_SVC[] = "SND";
    LOG_NOTICE(THIS_FUNC_SVC, "-- START -- " << THIS_FUNC_DESCRIPTION);

    if (strnlen(settings.waterius_key, WATERIUS_KEY_LEN) == 0) {
        LOG_NOTICE(THIS_FUNC_SVC, "NO Waterius key. SKIP");
        return false;
    };
    if (strnlen(settings.waterius_host, WATERIUS_HOST_LEN) == 0) {
        LOG_NOTICE(THIS_FUNC_SVC, "NO Waterus host. SKIP");
        return false;
    }

    // Set JSON body
    String jsonBody;
    StaticJsonDocument<JSON_BUFFER_SIZE> root;
    root["delta0"] =        cdata.delta0;
    root["delta1"] =        cdata.delta1;
    root["good"] =          data.diagnostic;
    root["boot"] =          data.service;
    root["ch0"] =           cdata.channel0;
    root["ch1"] =           cdata.channel1;
    root["imp0"] =          data.impulses0;
    root["imp1"] =          data.impulses1;
    root["version"] =       data.version;
    root["voltage"] =       (float)(data.voltage/1000.0);
    root["version_esp"] =   FIRMWARE_VERSION;
    root["key"] =           settings.waterius_key;
    root["resets"] =        data.resets;
    root["email"] =         settings.waterius_email;
    root["voltage_low"] =   cdata.low_voltage;
    root["voltage_diff"] =  cdata.voltage_diff;
    root["f"] =             settings.liters_per_impuls;
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
