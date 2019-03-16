#include "auth.h"
#include <WiFiClientSecureBearSSL.h>
#include <random>
#include "json.h"
#include "Logging.h"
#include <ArduinoJson.h>

#define JSON_BUFFER_SIZE 1000
#define REG_REQUEST_URL "http://192.168.199.2:8020/reg"
#define LOGIN_REQUEST_URL "http://192.168.199.2:8020/login"

bool UserAuth::login(char *email, char *token)
{
    LOG_NOTICE("LGN", "START :: Login");
    LOG_INFO("LGN", "Email: " << email);
    LOG_INFO("LGN", "Token: " << email);
    
    // Try to login
    String jsonBody;
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["email"] = email;
    root["token"] = token;
    root.printTo(jsonBody);
    bool login_result = WateriusJson::jsonPost(LOGIN_REQUEST_URL, jsonBody);

    LOG_INFO("LGN", "Login result -- " << (login_result ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("LGN", "END :: Login");

    return login_result;
}

bool UserAuth::reg(char *email)
{
    LOG_NOTICE("REG", "START :: Registration");
    LOG_INFO("REG", "Email: " << email);
    
    // Generate token
    char *token;
    randomSeed(time(nullptr));
    int salt = rand();
    auto x = BearSSL::HashSHA256();
    x.add(email, sizeof(email));
    x.add(&salt, sizeof(salt));
    x.end();
    token = (char *)x.hash();
    LOG_INFO("REG", "Token: " << token);
    
    
    // Register
    String jsonBody;
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["email"] = email;
    root["token"] = token;
    root.printTo(jsonBody);
    bool reg_result = WateriusJson::jsonPost(REG_REQUEST_URL, jsonBody);

    LOG_INFO("REG", "Registration result -- " << (reg_result ? "SUCCESS" : "ERROR"));
    LOG_NOTICE("REG", "END :: Registration");

    return reg_result;
}

