#include <ESP8266WiFi.h>
#include "ESP8266HTTPClient.h"
#include <ArduinoJson.h>
#include "Logging.h"
#include "utils.h"
#include "https_helpers.h"
#include "setup.h"
#include "core/connect_log.h"
#include "core/url.h"

SendStatus post_data(const String &url, const char *key, const char *email, const String &payload, JsonDocument &json_settings, int16_t &code)
{
    code = 0;
    void *pClient = nullptr;
    HTTPClient httpClient;
    SendStatus result = SEND_NO_CONNECTION;
    LOG_INFO(F("HTTP: Send JSON POST request"));
    LOG_INFO(F("HTTP: URL:") << url);
    LOG_INFO(F("HTTP: Body:") << payload);

    String proto = get_proto(url);
    LOG_INFO(F("HTTP: Protocol: ") << proto);

    // Set wc client
    if (proto == PROTO_HTTP)
    {
        LOG_INFO(F("HTTP: Create insecure client"));
        pClient = new WiFiClient;
    }
    else if (proto == PROTO_HTTPS)
    {
        LOG_INFO(F("HTTP: Create secure client"));
        pClient = new WiFiClientSecure;
        (*(WiFiClientSecure *)pClient).setInsecure(); // доверяем всем сертификатам
    }

    // HTTP settings
    httpClient.setTimeout(SERVER_TIMEOUT);
    httpClient.setReuse(false); // будет сразу закрывать подключение после отправки

    if (httpClient.begin(*(WiFiClient *)pClient, url))
    {
        httpClient.addHeader(F("Content-Type"), F("application/json"));
        if (key)
        {
            httpClient.addHeader(F("Waterius-Token"), key);
        }
        if (email)
        {
            httpClient.addHeader(F("Waterius-Email"), email);
        }
        LOG_INFO(F("HTTP: Post request"));

        int response_code = httpClient.POST(payload);
        LOG_INFO(F("HTTP: Response code: ") << response_code);
        code = response_code;

        // Отрицательный код — ошибка самого клиента: не разрешилось имя, не
        // открылось соединение, не дождались ответа
        if (response_code == 200)
        {
            result = SEND_OK;
        }
        else if (response_code > 0)
        {
            result = SEND_BAD_ANSWER;
        }

        String response_body = httpClient.getString();
        LOG_INFO(F("HTTP: Response body: ") << response_body);

        if (result == SEND_OK && response_body.length() > 2)
        {
            JsonDocument temp;
            DeserializationError error = deserializeJson(temp, response_body);
            if (!error && temp.is<JsonObject>())
            {
                for (JsonPairConst kv : temp.as<JsonObjectConst>())
                {
                    json_settings[kv.key()] = kv.value();
                }
                LOG_INFO(F("HTTP: Settings received from server"));
            }
        }

        httpClient.end();
        (*(WiFiClient *)pClient).stop();
    }

    if (proto == PROTO_HTTP)
    {
        delete (WiFiClient *)pClient;
    }
    else if (proto == PROTO_HTTPS)
    {
        delete (WiFiClientSecure *)pClient;
    }

    return result;
}

bool name_resolves(const char *host)
{
    IPAddress ip;
    return WiFi.hostByName(host, ip) == 1;
}

bool may_retry_send()
{
    return millis() < SEND_RETRY_LIMIT_MS;
}

int16_t connect_code(const char *url, int16_t code)
{
    char host[HOST_LEN];
    if (code == HTTPC_ERROR_CONNECTION_FAILED && url_host(url, host, sizeof(host)) && !name_resolves(host))
    {
        return CONNECT_CODE_DNS;
    }
    return code;
}
