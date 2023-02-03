#ifndef _WATERIUS_INIT_h
#define _WATERIUS_INIT_h
#include "setup.h"

#define TIME_FORMAT "%FT%T%z"
#define MAC_STR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_STR_HEX "%02X%02X%02X%02X%02X%02X"
#define PROTO_HTTPS "https"
#define PROTO_HTTP "http"

/*
Запишем 0 в конце буфера принудительно.
*/
inline void strncpy0(char *dest, const char *src, const size_t len)
{
    strncpy(dest, src, len - 1);
    dest[len - 1] = '\0';
}

extern bool setClock();

extern void print_wifi_mode();

extern void set_hostname();

extern String get_device_name();

extern String get_mac_address_hex();

extern  String get_current_time();

extern uint16_t get_checksum(const Settings &sett);

extern String get_proto(const String &url);

extern void remove_trailing_slash(String &topic);

extern bool is_waterius_site(const String &url);

extern bool is_mqtt(const Settings &sett);

extern bool is_blynk(const Settings &sett);

extern bool is_ha(const Settings &sett);

#endif