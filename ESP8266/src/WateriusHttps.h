#include "ESP8266HTTPClient.h"

/**
 * Both HTTP and HTTPS class
 **/
class WateriusHttps
{
    public:
    /**
     * -- TODO -- NOT CRITICAL --
     * Updates `trustAnchors` variable with new certificates.
     * Returns true if update was success.
     **/
    bool updateTrustAnchors(const char **certificates);

    /**
     * -- TODO HTTPS -- NOT CRITICAL --
     * JSON post request through WiFi.
     * Supports only HTTP.
     * Returns true if JSON post request was success.
     **/
    static bool sendPostRequestWithJson(const String &url, const String &body);

    /**
     * Get request through WiFi.
     * Supports HTTP.
     * Returns true if get request was success.
     **/
    static bool sendGetRequest(const String &url);
};
