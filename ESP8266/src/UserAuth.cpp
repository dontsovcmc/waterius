#include "UserAuth.h"
#include <WiFiClientSecureBearSSL.h>
#include <random>
#include "WateriusHttps.h"
#include "Logging.h"
#include <ArduinoJson.h>


#define JSON_BUFFER_SIZE 1000
#define LOGIN_REQUEST_URL "http://192.168.199.11:8020/login"


bool UserAuth::login(char *email, char *token)
{
    LOG_NOTICE("LGN", "START :: Login");
    LOG_INFO("LGN", "Email: " << email);
    LOG_INFO("LGN", "Token: " << token);
    
    // Try to login
    String jsonBody;
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["email"] = email;
    root["token"] = token;
    root.printTo(jsonBody);
    WateriusHttps::ResponseData responseData = WateriusHttps::sendPostRequestWithJson(LOGIN_REQUEST_URL, jsonBody);
    bool login_result = responseData.isSuccess && responseData.code == 200;

    LOG_INFO("LGN", "Login result -- " << (login_result ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("LGN", "END :: Login");

    return login_result;
}
