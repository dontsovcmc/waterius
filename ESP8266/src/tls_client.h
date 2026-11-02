#ifndef WATERIUS_TLS_CLIENT_H
#define WATERIUS_TLS_CLIENT_H

#include <WiFiClientSecure.h>

// anchors must outlive client and its connection. Never fall back to setInsecure().
bool configure_tls_client(WiFiClientSecure &client, BearSSL::X509List &anchors);

#endif
