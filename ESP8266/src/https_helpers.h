#ifndef HTTP_HELPERS_H_
#define HTTP_HELPERS_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include "core/blink.h"

/*
Возвращает не «получилось / не получилось», а причину: по ней светодиод
различает «нет связи с сервером» и «сервер ответил не 200» (core/blink.h).
*/
extern SendStatus post_data(const String &url, const char *key, const char *email, const String &payload, JsonDocument &json_settings);

#endif