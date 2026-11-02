#include "tls_client.h"
#include "tls_certificates.h"
#include "sync_time.h"
#include "Logging.h"

bool configure_tls_client(WiFiClientSecure &client, BearSSL::X509List &anchors)
{
    const time_t now = get_tls_time();
    if (!is_valid_time(now))
    {
        LOG_ERROR(F("TLS: time is unknown, refusing connection"));
        return false;
    }

    if (!anchors.append(TLS_CERTIFICATES) || anchors.getCount() != TLS_CERTIFICATE_COUNT)
    {
        LOG_ERROR(F("TLS: could not load trust anchors"));
        return false;
    }

    // BearSSL проверяет, кем выдан сертификат, имя сервера и срок действия.
    client.setTrustAnchors(&anchors);
    client.setX509Time(now);
    return true;
}
