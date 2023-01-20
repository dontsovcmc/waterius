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
static void* pClient = nullptr;

WateriusHttps::ResponseData WateriusHttps::sendJsonPostRequest(const String &url, const char *key, const char *email, const String &body)
{
    constexpr char THIS_FUNC_DESCRIPTION[] = "Send JSON POST request";
    LOG_INFO("-- START -- " << THIS_FUNC_DESCRIPTION);
    LOG_INFO("URL:\t" << url);
    LOG_INFO("Body:\t" << body);

    // Set wc client
    if (url.substring(0, 5) == "https")
    {
        pClient = new BearSSL::WiFiClientSecure;
        certs.append(lets_encrypt_x3_ca);
        certs.append(lets_encrypt_x4_ca);
        certs.append(cloud_waterius_ru_ca);
        ((BearSSL::WiFiClientSecure*)pClient)->setTrustAnchors(&certs);
    }
    else
    {
        pClient = new WiFiClient;
    }
    ((Client*)pClient)->setTimeout(SERVER_TIMEOUT);
    
    // HTTP settings
    httpClient.setTimeout(SERVER_TIMEOUT);
    httpClient.setReuse(false);

    // Check input data
    if (url.substring(0, 4) != "http")
    {
        LOG_ERROR(F("URL \"") << url << F("\" has not 'http' ('https')"));
    }
    if (((Client*)pClient)->available())
    {
        LOG_ERROR(F("Wi-Fi client is not available"));
    }

    LOG_INFO(F("Begin client"));
    // Request
    int responseCode = 0;
    String responseBody;
    if (httpClient.begin(*(WiFiClient*)pClient, url))
    {
        httpClient.addHeader(F("Content-Type"), F("application/json"));
        if (strnlen(key, WATERIUS_KEY_LEN))
        {
            httpClient.addHeader(F("Waterius-Token"), key);
        }
        if (strnlen(email, EMAIL_LEN))
        {
            httpClient.addHeader(F("Waterius-Email"), email);
        }
        responseCode = httpClient.POST(body);
        LOG_INFO(F("Response code:\t") << responseCode);
        responseBody = httpClient.getString();
        LOG_INFO(F("Response body:\t") << responseBody);
        httpClient.end();
        ((Client*)pClient)->stop();
    }
    else
    {
        LOG_ERROR(F("Cannot begin HTTP client"));
    }

    LOG_INFO(F("-- END --"));
    return WateriusHttps::ResponseData(responseCode, responseBody);
}

void WateriusHttps::generateSha256Token(char *token, const int token_len,
                                        const char *email)
{
    LOG_INFO(F("-- START -- ") << F("Generate SHA256 token from email"));

    auto x = BearSSL::HashSHA256();
    if (email != nullptr && strlen(email))
    {
        LOG_INFO(F("E-mail:\t") << email);
        x.add(email, strlen(email));
    }

    randomSeed(micros());
    uint32_t salt = rand();
    LOG_INFO(F("salt:\t") << salt);
    x.add(&salt, sizeof(salt));

    salt = getChipId();
    x.add(&salt, sizeof(salt));
    LOG_INFO(F("chip id: ") << salt);

    salt = ESP.getFlashChipId();
    x.add(&salt, sizeof(salt));
    LOG_INFO(F("flash id: ") << salt);
    x.end();
    unsigned char *hash = (unsigned char *)x.hash();

    static const char digits[] = "0123456789ABCDEF";

    for (int i = 0; i < x.len() && i < token_len - 1; i += 2, hash++)
    {
        token[i] = digits[*hash >> 4];
        token[i + 1] = digits[*hash & 0xF];
    }

    LOG_INFO(F("SHA256 token: ") << token);
    LOG_INFO(F("-- END --"));
}