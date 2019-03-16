#include "ESP8266HTTPClient.h"

class WateriusJson
{
    public:
    /**
     * -- TODO -- NOT CRITICAL --
     * Updates `trustAnchors` variable with new certificates
     * Returns true if update was success
     **/
    bool updateTrustAnchors(const char **certificates, int n);

    /**
     * Sends default HTTP JSON post request through WiFi
     * Returns true if JSON post request was success
     **/
    static bool jsonPost(const String &url, const String &body);
};