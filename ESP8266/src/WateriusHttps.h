#include "ESP8266HTTPClient.h"

/**
 * Both HTTP and HTTPS class
 **/
class WateriusHttps
{
    public:
    /**
     * Contains response data from request (REST API)
     **/
    struct ResponseData
    {
        const bool isSuccess;
        const int code;
        const String body;

        ResponseData(bool isSuccess, int code, String body) : isSuccess{isSuccess}, code{code}, body{body} {}
    };

    /**
     * JSON post request through WiFi.
     * Supports only HTTP.
     **/
    static ResponseData sendPostRequestWithJson(const String &url, const String &body);

    /**
     * Get request through WiFi.
     * Supports only HTTP.
     **/
    static ResponseData sendGetRequest(const String &url);
};
