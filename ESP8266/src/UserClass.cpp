#include "UserClass.h"
#include <WiFiClientSecureBearSSL.h>
#include <random>
#include "WateriusHttps.h"
#include "Logging.h"
#include <ArduinoJson.h>
#include "master_i2c.h"


#define JSON_BUFFER_SIZE 1000


bool UserClass::sendNewData(const Settings &settings, const SlaveData &data, const float &channel0, const float &channel1)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send new data";
    constexpr char THIS_FUNC_SVC[] = "SND";
    LOG_NOTICE(THIS_FUNC_SVC, "-- START -- " << THIS_FUNC_DESCRIPTION);
    
    // Set JSON body
    String jsonBody;
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["delta0"] =        (data.impulses0 - settings.impulses0_previous)*settings.liters_per_impuls;
    root["delta1"] =        (data.impulses1 - settings.impulses1_previous)*settings.liters_per_impuls;
    root["good"] =          data.diagnostic;
    root["boot"] =          data.version;
    root["ch0"] =           channel0;
    root["ch1"] =           channel1;
    root["version"] =       settings.version;
    root["voltage"] =       (float)(data.voltage/1000.0);
    root["version_esp"] =   FIRMWARE_VERSION;
    root["key"] =           settings.waterius_key;
    root["resets"] =        data.resets;
    root["email"] =         settings.waterius_email;
    root.printTo(jsonBody);

    if (strlen(settings.waterius_key) == 0) {
        LOG_NOTICE(THIS_FUNC_SVC, "NO Waterius key. SKIP");
        return false;
    };

    // Try to send
    WateriusHttps::ResponseData responseData = WateriusHttps::sendJsonPostRequest(settings.waterius_host, jsonBody);
    bool send_result = responseData.isSuccess && responseData.code == 200;

    LOG_INFO(THIS_FUNC_SVC, "Send result:\t" << (send_result ? "Success" : "Error"));
    LOG_NOTICE(THIS_FUNC_SVC, "-- END --");

    return send_result;
}
