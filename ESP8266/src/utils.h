#ifndef _WATERIUS_INIT_h
#define _WATERIUS_INIT_h
#include "setup.h"

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
    dest[len - 1] = 0;
}

extern String get_device_name();

extern String get_ap_name();

extern String get_mac_address_hex();

extern uint16_t get_checksum(const Settings &sett);

extern String get_proto(const String &url);

extern void remove_trailing_slash(String &topic);

extern bool is_waterius_site(const Settings &sett);

extern bool is_http(const Settings &sett);

extern bool is_mqtt(const Settings &sett);

extern bool is_ha(const Settings &sett);

extern bool is_dhcp(const Settings &sett);

extern bool is_https(const char *url);

extern void log_system_info();

extern void generateSha256Token(char *token, const int token_len, const char *email);

extern void blink_led(int count = 1, int period = 300, int duty = 150);

extern DataType data_type_by_name(uint8_t counter_name);

#endif