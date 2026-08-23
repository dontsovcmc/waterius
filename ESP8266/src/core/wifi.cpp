#include "wifi.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BSSID_LEN 6
#define BSSID_HEX_DIGITS (BSSID_LEN * 2)

static int hex_digit(const char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

uint8_t parse_wifi_channel(const char *value)
{
    if (value == nullptr)
        return 0;

    long channel = atol(value);

    if (channel < WIFI_CHANNEL_MIN || channel > WIFI_CHANNEL_MAX)
        return 0;

    return (uint8_t)channel;
}

bool parse_bssid(const char *value, uint8_t out[6])
{
    memset(out, 0, BSSID_LEN);

    if (value == nullptr)
        return false;

    uint8_t bytes[BSSID_LEN] = {0};
    int digits = 0;

    for (size_t i = 0; value[i]; ++i)
    {
        const char c = value[i];

        // Разделители байтов: и двоеточие, и дефис — какой пришёл, такой и есть
        if (c == ':' || c == '-')
            continue;

        const int digit = hex_digit(c);
        if (digit < 0 || digits >= BSSID_HEX_DIGITS)
            return false;

        bytes[digits / 2] = (uint8_t)(bytes[digits / 2] << 4 | digit);
        digits++;
    }

    if (digits != BSSID_HEX_DIGITS)
        return false;

    memcpy(out, bytes, BSSID_LEN);
    return true;
}

bool has_bssid(const uint8_t bssid[6])
{
    for (int i = 0; i < BSSID_LEN; ++i)
    {
        if (bssid[i])
            return true;
    }
    return false;
}

uint8_t ap_channel(const uint8_t channel)
{
    if (channel < WIFI_CHANNEL_MIN || channel > WIFI_CHANNEL_MAX)
        return WIFI_CHANNEL_MIN;

    return channel;
}
