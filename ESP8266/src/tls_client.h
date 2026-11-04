#ifndef WATERIUS_TLS_CLIENT_H
#define WATERIUS_TLS_CLIENT_H

#include <WiFiClientSecure.h>

// Сертификаты должны оставаться в памяти до закрытия соединения.
bool configure_tls_client(WiFiClientSecure &client, BearSSL::X509List &anchors);

#endif
