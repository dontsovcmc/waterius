#include "WateriusHttps.h"
#include <utility>
#include "time.h"
#include "Logging.h"
#include "cert.h"
#include "utils.h"
#include "WiFiClientSecureBearSSL.h"
#include "setup.h"
#include "porting.h"

BearSSL::X509List certs;
HTTPClient httpClient;
WiFiClient wifiClient;
BearSSL::WiFiClientSecure wifiTlsClient;

#define JSON_BUFFER_SIZE 500

WateriusHttps::ResponseData WateriusHttps::sendJsonPostRequest(const String &url, const char *key, const char *email, const String &body)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send JSON POST request";
    LOG_INFO(FPSTR(S_RQT), "-- START -- " << THIS_FUNC_DESCRIPTION);
    LOG_INFO(FPSTR(S_RQT), "URL:\t" << url);
    LOG_INFO(FPSTR(S_RQT), "Body:\t" << body);
    
    // Set wc client
    WiFiClient *wc;
    if (url.substring(0, 5) == "https") {
        wc = &wifiTlsClient;
        certs.append(lets_encrypt_x3_ca);
        certs.append(lets_encrypt_x4_ca);
        certs.append(cloud_waterius_ru_ca);
        wifiTlsClient.setTrustAnchors(&certs);

        if (!setClock()) {  
            LOG_ERROR(FPSTR(S_RQT), "SetClock FAILED");
            return WateriusHttps::ResponseData();
        }
    } else {
        wc = &wifiClient;
    }
    wc->setTimeout(SERVER_TIMEOUT);

    // HTTP settings
    HTTPClient *hc = &httpClient;
    hc->setTimeout(SERVER_TIMEOUT);
    hc->setReuse(false);
    
    // Check input data
    if (url.substring(0, 4) != "http") {
        LOG_ERROR(FPSTR(S_RQT), F("URL \"") << url << F("\" has not 'http' ('https')"));
    }
    if (wc->available()) {
        LOG_ERROR(FPSTR(S_RQT), F("Wi-Fi client is not available"));
    }
    
    
    LOG_INFO(FPSTR(S_RQT), F("Begin client"));
    // Request
    int responseCode = 0;
    String responseBody;
    if (hc->begin(*wc, url)) {
        hc->addHeader(F("Content-Type"), F("application/json"));
        if (strnlen(key, WATERIUS_KEY_LEN)) {
            hc->addHeader(F("Waterius-Token"), key);
        }
        if (strnlen(email, EMAIL_LEN)) {
            hc->addHeader(F("Waterius-Email"), email);
        }
        responseCode = hc->POST(body);
        LOG_INFO(FPSTR(S_RQT), F("Response code:\t") << responseCode);
        responseBody = hc->getString();
        LOG_INFO(FPSTR(S_RQT), F("Response body:\t") << responseBody);
        hc->end();
        wc->stop();
    } else {
        LOG_ERROR(FPSTR(S_RQT), F("Cannot begin HTTP client"));
    }

    LOG_INFO(FPSTR(S_RQT), F("-- END --"));
    return WateriusHttps::ResponseData(responseCode, responseBody);
}


void WateriusHttps::generateSha256Token(char *token, const int token_len,
                                        const char *email)
{
    constexpr char THIS_FUNC_SVC[] = "TKN";
    LOG_INFO(THIS_FUNC_SVC, F("-- START -- ") << F("Generate SHA256 token from email"));
    
    auto x = BearSSL::HashSHA256();
    if (email != nullptr && strlen(email)) {
        LOG_INFO(THIS_FUNC_SVC, F("E-mail:\t") << email);
        x.add(email, strlen(email));
    }
    
    randomSeed(micros());
    uint32_t salt = rand();
    LOG_INFO(THIS_FUNC_SVC, F("salt:\t") << salt);
    x.add(&salt, sizeof(salt));

    salt = getChipId();
    x.add(&salt, sizeof(salt));
    LOG_INFO(THIS_FUNC_SVC, F("chip id: ") << salt);
    
    salt = ESP.getFlashChipId();
    x.add(&salt, sizeof(salt));
    LOG_INFO(THIS_FUNC_SVC, F("flash id: ") << salt);
    x.end();
    unsigned char *hash = (unsigned char *)x.hash();

    static const char digits[] = "0123456789ABCDEF";

    for (int i=0; i < x.len() && i < token_len-1; i +=2, hash++) {
        token[i] = digits[*hash >> 4];
        token[i+1] = digits[*hash & 0xF];
    }

    LOG_INFO(THIS_FUNC_SVC, F("SHA256 token: ") << token);
    LOG_INFO(THIS_FUNC_SVC, F("-- END --"));
}
