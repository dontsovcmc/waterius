#ifndef HTTP_HELPERS_H_
#define HTTP_HELPERS_H_

#include <Arduino.h>

extern bool post_data(const String &url, const char *key, const char *email, const String &payload);

#endif