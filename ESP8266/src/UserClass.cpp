#include "UserClass.h"
#include "WateriusHttps.h"
#include "Logging.h"
#include <ArduinoJson.h>


#define JSON_BUFFER_SIZE 500


bool UserClass::sendNewData(const Settings &settings, const SlaveData &data, const CalculatedData &cdata)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send new data";
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_SND)); LOG(F("-- START -- ")); LOG(THIS_FUNC_DESCRIPTION);
    #endif

    if (strnlen(settings.waterius_key, WATERIUS_KEY_LEN) == 0) {
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_SND)); LOG(F("SKIP"));
        #endif
        return false;
    };
    if (strnlen(settings.waterius_host, WATERIUS_HOST_LEN) == 0) {
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_SND)); LOG(F("SKIP"));
        #endif
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
    root["f0"] =            settings.factor0;
    root["f1"] =            settings.factor1;
    root["rssi"] =          cdata.rssi;
    root["waketime"] =      settings.wake_time;
    root["setuptime"] =     settings.setup_time;
    root["adc0"] =          data.adc0;
    root["adc1"] =          data.adc1;
    root["period_min"] =    settings.wakeup_per_min;
    root["serial0"] =       settings.serial0;
    root["serial1"] =       settings.serial1;

    serializeJson(root, jsonBody);
    //JSON size:  355  0.10.3
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_SND)); LOG(F("JSON size:\t")); LOG(jsonBody.length());
    #endif
    
    // Try to send
    WateriusHttps::ResponseData responseData = WateriusHttps::sendJsonPostRequest(
        settings.waterius_host, settings.waterius_key, settings.waterius_email, jsonBody);

    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_SND)); LOG(F("Send HTTP code:\t")); LOG(responseData.code);
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_SND)); LOG(F("-- END --"));
    #endif

    return responseData.code == 200;
}
