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
        const bool isSuccess{false};
        const int code{0};
        const String body{};

        ResponseData() {}
        ResponseData(bool isSuccess, int code, String body) : isSuccess{isSuccess}, code{code}, body{body} {}
    };

    /**
     * JSON post request through WiFi.
     * Supports HTTPS.
     **/
    static ResponseData sendJsonPostRequest(const String &url, const String &body);

    /**
     * Generate SHA256 Token from random & email.
     **/
    static void generateToken(const char *email, char *hash, const int hash_len);
};
