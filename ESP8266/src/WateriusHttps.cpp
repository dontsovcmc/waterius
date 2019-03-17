#include "WateriusHttps.h"
#include "time.h"
#include "Logging.h"
#include "cert.h"
#include "utils.h"
#include <ArduinoJson.h>
#include "WifiClientSecure.h"
#include "ESP8266HTTPClient.h"


#define HTTP_TIMEOUT_MILLES 5000
#define JSON_BUFFER_SIZE 1000


bool WateriusHttps::sendPostRequestWithJson(const String &url, const String &body)
{
    String method_name;
    WiFiClient wifiClient;

    LOG_NOTICE("RQT", "START :: HTTP JSON POST request");
    LOG_INFO("RQT", "url: " << url);
    LOG_INFO("RQT", "body: " << body);

    // Check input data
    if (url.substring(0, 4) != "http") {
        LOG_WARNING("RQT", "URL \"" << url << "\" has not 'http'");
    }
    if (wifiClient.available()) {
        LOG_WARNING("RQT", "Wi-Fi client is not available");
    }
    if (!setClock()) {
        LOG_WARNING("RQT", "SetClock fail ???");
    }

    // wifiClient settings
    wifiClient.setTimeout(SERVER_TIMEOUT);

    // HTTP settings
    HTTPClient httpClient;
    httpClient.setTimeout(HTTP_TIMEOUT_MILLES);
    httpClient.setReuse(false);

    bool return_result = true;
    if (httpClient.begin(wifiClient, url)) {
        // Request
        httpClient.addHeader("Content-Type", "application/json");
        int responseCode = httpClient.POST(body);
        LOG_INFO("RQT", "Response code -- " << responseCode);
        String responseBody = httpClient.getString();
        LOG_INFO("RQT", "Response body -- " << body);
        httpClient.end();
        wifiClient.stop();

        // Response processing
        if (responseCode == HTTP_CODE_OK) {
            StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(responseBody);
            if (!root.success()) {
                LOG_INFO("RQT", "Response status -- ERROR");
                return_result = false;
            }
        } else {
            return_result = false;
        }
    } else {
        return_result = false;
    }

    LOG_INFO("RQT", "Result -- " << (return_result ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("RQT", "END :: HTTP JSON POST request");

    return return_result;
}

bool WateriusHttps::sendGetRequest(const String &url)
{
    String method_name;
    WiFiClient wifiClient;

    LOG_NOTICE("RQT", "START :: HTTP GET request");
    LOG_INFO("RQT", "url: " << url);

    // Check input data
    if (url.substring(0, 4) != "http") {
        LOG_WARNING("RQT", "URL \"" << url << "\" has not 'http'");
    }
    if (wifiClient.available()) {
        LOG_WARNING("RQT", "Wi-Fi client is not available");
    }
    if (!setClock()) {
        LOG_WARNING("RQT", "SetClock fail ???");
    }

    // wifiClient settings
    wifiClient.setTimeout(SERVER_TIMEOUT);

    // HTTP settings
    HTTPClient httpClient;
    httpClient.setTimeout(HTTP_TIMEOUT_MILLES);
    httpClient.setReuse(false);

    bool return_result = true;
    if (httpClient.begin(wifiClient, url)) {
        // Request
        int responseCode = httpClient.GET();
        LOG_INFO("RQT", "Response code -- " << responseCode);
        String responseBody = httpClient.getString();
        LOG_INFO("RQT", "Response body -- " << responseBody);
        httpClient.end();
        wifiClient.stop();

        // Response processing
        if (responseCode == HTTP_CODE_OK) {
            StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(responseBody);
            if (!root.success()) {
                LOG_INFO("RQT", "Response status -- ERROR");
                return_result = false;
            }
        } else {
            return_result = false;
        }
    } else {
        return_result = false;
    }

    LOG_INFO("RQT", "Result -- " << (return_result ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("RQT", "END :: HTTP GET request");

    return return_result;
}
