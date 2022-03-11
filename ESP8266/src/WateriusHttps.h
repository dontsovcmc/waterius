#include "ESP8266HTTPClient.h"

/**
 * Both HTTP and HTTPS class.
 **/
class WateriusHttps
{
    public:
    /**
     * Contains response data from request (REST API).
     **/
    struct ResponseData
    {
        const int code{0};
        const String body{};

        ResponseData() {}
        ResponseData(int code, String body) : code{code}, body{body} {}
    };

    /**
     * JSON post request through WiFi.
     * Supports HTTPS.
     **/
    static ResponseData sendJsonPostRequest(const String &url, const char *key, const char *email, const String &body);

    /**
     * Generate SHA256 Token from random & email.
     **/
    static void generateSha256Token(char *token, const int token_len, 
                                    const char *email);
};
