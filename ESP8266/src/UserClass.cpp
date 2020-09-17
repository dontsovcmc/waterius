#include "UserClass.h"
#include "WateriusHttps.h"
#include "Logging.h"
#include <ArduinoJson.h>
#include "utils.h"


#define JSON_BUFFER_SIZE 600


bool UserClass::sendNewData(const Settings &settings, const SlaveData &data, const CalculatedData &cdata)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send new data";
    LOG_INFO(FPSTR(S_SND), "-- START -- " << THIS_FUNC_DESCRIPTION);

    if (strnlen(settings.waterius_key, WATERIUS_KEY_LEN) == 0) {
        LOG_INFO(FPSTR(S_SND), F("SKIP"));
        return false;
    };
    if (strnlen(settings.waterius_host, WATERIUS_HOST_LEN) == 0) {
        LOG_INFO(FPSTR(S_SND), F("SKIP"));
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
    root["rssi"] =          cdata.rssi;
    root["waketime"] =      settings.wake_time;
    root["setuptime"] =     settings.setup_time;
    root["adc0"] =          data.adc0;
    root["adc1"] =          data.adc1;

    if (data.model == WATERIUS_4C2W) {
        root["model"] =         data.model;
        root["delta2"] =        cdata.delta2;
        root["delta3"] =        cdata.delta3;
        root["ch2"] =           cdata.channel2;
        root["ch3"] =           cdata.channel3;
        root["imp2"] =          data.impulses2;
        root["imp3"] =          data.impulses3;
        root["adc2"] =          data.adc2;
        root["adc3"] =          data.adc3;
        root["waterleak1"] =    wl_state((WaterLeak_e)data.statewl1);
        root["waterleak2"] =    wl_state((WaterLeak_e)data.statewl2);
        root["adcwl1"] =        data.adcwl1;
        root["adcwl2"] =        data.adcwl2;
    }

    serializeJson(root, jsonBody);
    LOG_INFO(FPSTR(S_SND), "JSON size:\t" << jsonBody.length());
    
    // Try to send
    WateriusHttps::ResponseData responseData = WateriusHttps::sendJsonPostRequest(
        settings.waterius_host, settings.waterius_key, settings.waterius_email, jsonBody);

    LOG_INFO(FPSTR(S_SND), "Send HTTP code:\t" << responseData.code);
    LOG_INFO(FPSTR(S_SND), "-- END --");

    return responseData.code == 200;
}
