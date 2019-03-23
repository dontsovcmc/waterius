#include "WateriusHttps.h"
#include "time.h"
#include "Logging.h"
#include "cert.h"
#include "utils.h"
#include <ArduinoJson.h>
#include "WifiClientSecure.h"
#include "ESP8266HTTPClient.h"

#include <utility>

BearSSL::X509List certs;
HTTPClient httpClient;
WiFiClient wifiClient;
WiFiClientSecure wifiTlsClient;

#define HTTP_TIMEOUT_MILLES 5000
#define JSON_BUFFER_SIZE 1000

WateriusHttps::ResponseData WateriusHttps::sendJsonPostRequest(const String &url, const String &body)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send JSON POST request";
    constexpr char THIS_FUNC_SVC[] = "RQT";
    LOG_NOTICE(THIS_FUNC_SVC, "-- START -- " << THIS_FUNC_DESCRIPTION);
    LOG_INFO(THIS_FUNC_SVC, "URL:\t" << url);
    LOG_INFO(THIS_FUNC_SVC, "Body:\t" << body);
    
    // Set wc client
    WiFiClient *wc;
    if (url.substring(0, 5) == "https") {
        wc = &wifiTlsClient;
        certs.append(le_ca_cert);
        certs.append(lets_encrypt_x3_ca);
        certs.append(lets_encrypt_x4_ca);
        certs.append(cloud_waterius_ru_ca);
        wifiTlsClient.setTrustAnchors(&certs);

        if (!setClock()) {  
            LOG_WARNING(THIS_FUNC_SVC, "SetClock fail ???");
            return WateriusHttps::ResponseData();
        }
    } else {
        wc = &wifiClient;
    }
    wc->setTimeout(SERVER_TIMEOUT);

    // HTTP settings
    HTTPClient *hc = &httpClient;
    hc->setTimeout(HTTP_TIMEOUT_MILLES);
    hc->setReuse(false);
    
    // Check input data
    if (url.substring(0, 4) != "http") {
        LOG_WARNING(THIS_FUNC_SVC, "URL \"" << url << "\" has not 'http' ('https')");
    }
    if (wc->available()) {
        LOG_WARNING(THIS_FUNC_SVC, "Wi-Fi client is not available");
    }

    // Request
    bool responseResult = false;
    int responseCode = 0;
    String responseBody;
    if (hc->begin(*wc, url)) {
        hc->addHeader("Content-Type", "application/json");
        responseCode = hc->POST(body);
        LOG_INFO(THIS_FUNC_SVC, "Response code:\t" << responseCode);
        responseBody = hc->getString();
        LOG_INFO(THIS_FUNC_SVC, "Response body:\t" << responseBody);
        hc->end();
        wc->stop();
        responseResult = true;
    } else {
        LOG_ERROR(THIS_FUNC_SVC, "Cannot begin HTTP client");
    }

    LOG_INFO(THIS_FUNC_SVC, "Result:\t" << (responseResult ? "Success" : "Error"));
    LOG_NOTICE(THIS_FUNC_SVC, "-- END --");
    return WateriusHttps::ResponseData(responseResult, responseCode, responseBody);
}


void WateriusHttps::generateSha256Token(char *hash, const int hash_len,
                                        const char *email, const int email_len)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Generate SHA256 token from email";
    constexpr char THIS_FUNC_SVC[] = "TKN";
    LOG_NOTICE(THIS_FUNC_SVC, "-- START -- " << THIS_FUNC_DESCRIPTION);
    LOG_INFO(THIS_FUNC_SVC, "E-mail:\t" << email);
    
    randomSeed(time(nullptr));
    int salt = rand();
    auto x = BearSSL::HashSHA256();
    x.add(email, sizeof(email));
    x.add(&salt, sizeof(salt));
    x.end();
    snprintf(hash, x.len(), "%X", x.hash());

    LOG_INFO(THIS_FUNC_SVC, "SHA256 token: " << hash);
    LOG_NOTICE(THIS_FUNC_SVC, "-- END --");
}