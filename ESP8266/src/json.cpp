#include "json.h"
#include "time.h"
#include "Logging.h"
#include "cert.h"
#include "utils.h"
#include <ArduinoJson.h>
#include "WifiClientSecure.h"
#include "ESP8266HTTPClient.h"


#define HTTP_TIMEOUT_MILLES 5000
#define JSON_BUFFER_SIZE 1000


/**
 * Sends default HTTP JSON post request through WiFi
 * Returns true if JSON post request was success
 **/
bool WateriusJson::jsonPost(const String &url, const String &body)
{
    String method_name;
    WiFiClient wifiClient;

    LOG_NOTICE("JSN", "START :: HTTP JSON POST request");
    LOG_INFO("JSN", "url: " << url);
    LOG_INFO("JSN", "body: " << body);

    // Check input data
    if (url.substring(0, 4) != "http") {
        LOG_WARNING("JSN", "URL \"" << url << "\" has not 'http'");
    }
    if (wifiClient.available()) {
        LOG_WARNING("JSN", "Wi-Fi client is not available");
    }
    if (!setClock()) {
        LOG_WARNING("JSN", "SetClock fail ???");
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
        LOG_INFO("JSN", "Response code -- " << responseCode);
        String responseBody = httpClient.getString();
        LOG_INFO("JSN", "Response body -- " << body);
        httpClient.end();
        wifiClient.stop();

        // Response processing
        if (responseCode == HTTP_CODE_OK) {
            StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(body);
            if (!root.success()) {
                LOG_INFO("PST", "Response status -- ERROR");
                return_result = false;
            }
        } else {
            return_result = false;
        }
    }

    LOG_INFO("JSN", "Result -- " << (return_result ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("JSN", "END :: HTTP JSON POST request");

    return return_result;
}
