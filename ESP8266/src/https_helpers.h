#ifndef HTTP_HELPERS_H_
#define HTTP_HELPERS_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include "core/blink.h"

/*
Возвращает не «получилось / не получилось», а причину: по ней светодиод
различает «нет связи с сервером» и «сервер ответил не 200» (core/blink.h).
*/
extern SendStatus post_data(const String &url, const char *key, const char *email, const String &payload, JsonDocument &json_settings, int16_t &code);

/*
Код для журнала ошибок связи: соединение не открылось, а имя сервера не
разрешается - значит, нет интернета или DNS (core/connect_log.h).
*/
extern int16_t connect_code(const char *url, int16_t code);
extern bool name_resolves(const char *host);

#endif