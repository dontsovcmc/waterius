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


WateriusHttps::ResponseData WateriusHttps::sendPostRequestWithJson(const String &url, const String &body)
{
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

    bool responseResult = true;
    int responseCode = 0;
    String responseBody;
    if (httpClient.begin(wifiClient, url)) {
        // Request
        httpClient.addHeader("Content-Type", "application/json");
        responseCode = httpClient.POST(body);
        LOG_INFO("RQT", "Response code -- " << responseCode);
        responseBody = httpClient.getString();
        LOG_INFO("RQT", "Response body -- " << body);
        httpClient.end();
        wifiClient.stop();
    } else {
        responseResult = false;
        LOG_ERROR("RQT", "Cannot begin HTTP client");
    }

    LOG_INFO("RQT", "Result -- " << (responseResult ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("RQT", "END :: HTTP JSON POST request");

    return WateriusHttps::ResponseData(responseResult, responseCode, responseBody);
}

WateriusHttps::ResponseData WateriusHttps::sendGetRequest(const String &url)
{
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

    bool responseResult = true;
    int responseCode = 0;
    String responseBody;
    if (httpClient.begin(wifiClient, url)) {
        // Request
        int responseCode = httpClient.GET();
        LOG_INFO("RQT", "Response code -- " << responseCode);
        String responseBody = httpClient.getString();
        LOG_INFO("RQT", "Response body -- " << responseBody);
        httpClient.end();
        wifiClient.stop();
    } else {
        responseResult = false;
        LOG_ERROR("RQT", "Cannot begin HTTP client");
    }

    LOG_INFO("RQT", "Result -- " << (responseResult ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("RQT", "END :: HTTP GET request");

    return WateriusHttps::ResponseData(responseResult, responseCode, responseBody);
}
