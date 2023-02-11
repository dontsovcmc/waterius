#include "WateriusHttps.h"
#include "time.h"
#include "Logging.h"
#include "cert.h"
#include "utils.h"
#include "WiFiClientSecureBearSSL.h"
#include "setup.h"
#include "porting.h"

WateriusHttps::ResponseData WateriusHttps::sendJsonPostRequest(const char *url, const char *key, const char *email, const String &payload)
{
    BearSSL::X509List certs;
    HTTPClient httpClient;
    void *pClient = nullptr;
    BearSSL::CertStore certStore;
    
    LOG_INFO(F("-- START -- ") << F("Send JSON POST request"));
    LOG_INFO(F("URL:\t") << url);
    LOG_INFO(F("Body:\t") << payload);

    String proto = get_proto(String(url));
    LOG_INFO(F("Protocol:\t") << proto);

    // Set wc client
    if ((proto == PROTO_HTTP))
    {
        LOG_INFO(F("Create insecure client"));
        pClient = new WiFiClient;
        LOG_INFO(F("Insecure client created"));
    }
    else if (proto == PROTO_HTTPS)
    {
        LOG_INFO(F("Create secure client"));
        pClient = new BearSSL::WiFiClientSecure;
        if (is_waterius_site(String(url))) {
            // проверяем валидность сертифкат сайта ватериуса
            certs.append(cloud_waterius_ru_ca);
            (*(BearSSL::WiFiClientSecure *)pClient).setTrustAnchors(&certs);
            // если нужно будет добавить еще сертификатов то лучше их не добавлять в certs
            // так как каждый из них будет дополнительно загружен в память
            // а один сертификат занимает больше 1.5кБ
            // нужно реализовывать через BearSSL::CertStore
            // пример реализации https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/BearSSL_CertStore.ino
        }
        else {
            // если не сайт ватериуса, то самоподписанные сертификаты будут работать
            //(*(BearSSL::WiFiClientSecure *)pClient).allowSelfSignedCerts();
    
            (*(BearSSL::WiFiClientSecure *)pClient).setInsecure();
        }

        //The per-connection buffers are approximately 22KB in size
        // https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/bearssl-client-secure-class.rst
        // поэтому обязательно нужно уменьшиить буферы 
        // иначе клиент отъест большье 22 кб под буфер
        // и вся память закончится и срабоатает ватчдог и перезапустит есп. 
        (*(BearSSL::WiFiClientSecure *)pClient).setBufferSizes(1024, 1024); 
                LOG_INFO(F("Secure client created"));
    }
    else
    {
        LOG_ERROR(F("URL \"") << url << F("\" has not 'http' ('https')"));
        return WateriusHttps::ResponseData();
    }

    (*(WiFiClient *)pClient).setTimeout(SERVER_TIMEOUT);

    // HTTP settings
    httpClient.setTimeout(SERVER_TIMEOUT);
    httpClient.setReuse(false);

    LOG_INFO(F("Begin client"));
    // Request
    int responseCode = 0;
    String responseBody;
    if (httpClient.begin(*(WiFiClient *)pClient, String(url)))
    {
        LOG_INFO(F("Begin HTTP client succesfully"));
        httpClient.addHeader(F("Content-Type"), F("application/json"));
        if (key[0])
        {
            httpClient.addHeader(F("Waterius-Token"), key);
        }
        if (email[0])
        {
            httpClient.addHeader(F("Waterius-Email"), email);
        }
        LOG_INFO(F("Post request"));
        responseCode = httpClient.POST(payload);
        LOG_INFO(F("Response code:\t") << responseCode);
        responseBody = httpClient.getString();
        LOG_INFO(F("Response body:\t") << responseBody);
        httpClient.end();
        (*(WiFiClient *)pClient).stop();
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