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
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_RQT)); LOG(F("-- START -- ")); LOG(THIS_FUNC_DESCRIPTION);
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_RQT)); LOG(F("URL:\t")); LOG(url);
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_RQT)); LOG(F("Body:\t")); LOG(body);
    #endif
    
    // Set wc client
    WiFiClient *wc;
    if (url.substring(0, 5) == "https") {
        wc = &wifiTlsClient;
        certs.append(lets_encrypt_x3_ca);
        certs.append(lets_encrypt_x4_ca);
        certs.append(cloud_waterius_ru_ca);
        wifiTlsClient.setTrustAnchors(&certs);

        if (!setClock()) {
            #if LOGLEVEL>=0
            LOG_START(FPSTR(S_ERROR) ,FPSTR(S_RQT)); LOG(F("SetClock FAILED"));
            #endif
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
        #if LOGLEVEL>=0
        LOG_START(FPSTR(S_ERROR) ,FPSTR(S_RQT)); LOG(F("URL \"")); LOG(url); LOG(F("\" has not 'http' ('https')"));
        #endif
    }
    if (wc->available()) {
        #if LOGLEVEL>=0
        LOG_START(FPSTR(S_ERROR) ,FPSTR(S_RQT)); LOG(F("Wi-Fi client is not available"));
        #endif
    }
    
    
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_RQT)); LOG(F("Begin client"));
    #endif
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
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_RQT)); LOG(F("Response code:\t")); LOG(responseCode);
        #endif
        responseBody = hc->getString();
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(S_RQT)); LOG(F("Response body:\t")); LOG(responseBody);
        #endif
        hc->end();
        wc->stop();
    } else {
        #if LOGLEVEL>=0
        LOG_START(FPSTR(S_ERROR) ,FPSTR(S_RQT)); LOG(F("Cannot begin HTTP client"));
        #endif
    }

    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(S_RQT)); LOG(F("-- END --"));
    #endif
    return WateriusHttps::ResponseData(responseCode, responseBody);
}


void WateriusHttps::generateSha256Token(char *token, const int token_len,
                                        const char *email)
{
    constexpr char THIS_FUNC_SVC[] = "TKN";
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(THIS_FUNC_SVC)); LOG(F("-- START -- Generate SHA256 token from email"));
    #endif
    
    auto x = BearSSL::HashSHA256();
    if (email != nullptr && strlen(email)) {
        #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(THIS_FUNC_SVC)); LOG(F("E-mail:\t")); LOG(email);
        #endif
        x.add(email, strlen(email));
    }
    
    randomSeed(micros());
    uint32_t salt = rand();
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(THIS_FUNC_SVC)); LOG(F("salt:\t")); LOG(salt);
    #endif
    x.add(&salt, sizeof(salt));

    salt = getChipId();
    x.add(&salt, sizeof(salt));
    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(THIS_FUNC_SVC)); LOG(F("chip id: ")); LOG(salt);
    #endif
    
    salt = ESP.getFlashChipId();
    x.add(&salt, sizeof(salt));
    #if LOGLEVEL>=1
        LOG_START(FPSTR(S_INFO) ,FPSTR(THIS_FUNC_SVC)); LOG(F("flash id: ")); LOG(salt);
        #endif
    x.end();
    unsigned char *hash = (unsigned char *)x.hash();

    static const char digits[] = "0123456789ABCDEF";

    for (int i=0; i < x.len() && i < token_len-1; i +=2, hash++) {
        token[i] = digits[*hash >> 4];
        token[i+1] = digits[*hash & 0xF];
    }

    #if LOGLEVEL>=1
    LOG_START(FPSTR(S_INFO) ,FPSTR(THIS_FUNC_SVC)); LOG(F("SHA256 token: ")); LOG(token);
    LOG_START(FPSTR(S_INFO) ,FPSTR(THIS_FUNC_SVC)); LOG(F("-- END --"));
    #endif
}
