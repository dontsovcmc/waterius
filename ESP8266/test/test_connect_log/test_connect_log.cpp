#include <gtest/gtest.h>
#include <string>
#include "core/connect_log.h"

/*
Журнал ошибок связи. Счётчик сравнивает получатель: только по нему новая ошибка
отличается от прошлой с тем же кодом, поэтому журнал и счётчик проверяем вместе.
*/

static std::string text(const Settings &sett)
{
    char out[CONNECT_LOG_TEXT_LEN];
    connect_log_text(sett, out, sizeof(out));
    return out;
}

TEST(ConnectLog, EmptyIsEmptyString)
{
    Settings sett;
    EXPECT_EQ(text(sett), "");
    EXPECT_EQ(sett.wifi_connect_errors, 0);
}

TEST(ConnectLog, OldestFirst)
{
    Settings sett;
    connect_log_push(sett, CONNECT_WIFI, 201);
    connect_log_push(sett, CONNECT_WATERIUS, -1);
    EXPECT_EQ(text(sett), "wifi:201,wtr:-1");
}

TEST(ConnectLog, KeepsLastThree)
{
    Settings sett;
    connect_log_push(sett, CONNECT_WIFI, 201);
    connect_log_push(sett, CONNECT_WATERIUS, -1);
    connect_log_push(sett, CONNECT_HTTP, 500);
    connect_log_push(sett, CONNECT_MQTT, -2);
    EXPECT_EQ(text(sett), "wtr:-1,http:500,mqtt:-2");
    EXPECT_EQ(sett.wifi_connect_errors, 4);
}

TEST(ConnectLog, SameCodeStillCounts)
{
    Settings sett;
    connect_log_push(sett, CONNECT_WIFI, 201);
    connect_log_push(sett, CONNECT_WIFI, 201);
    connect_log_push(sett, CONNECT_WIFI, 201);
    const std::string before = text(sett);

    connect_log_push(sett, CONNECT_WIFI, 201);

    EXPECT_EQ(text(sett), before);
    EXPECT_EQ(sett.wifi_connect_errors, 4);
}

TEST(ConnectLog, CounterGoesPastOneByte)
{
    Settings sett;
    for (int i = 0; i < 1344; ++i)
    {
        connect_log_push(sett, CONNECT_WIFI, 201);
    }
    EXPECT_EQ(sett.wifi_connect_errors, 1344);
}

TEST(ConnectLog, CodesKeepTheirValue)
{
    Settings sett;
    connect_log_push(sett, CONNECT_HTTP, 503);
    connect_log_push(sett, CONNECT_WATERIUS, CONNECT_CODE_DNS);
    connect_log_push(sett, CONNECT_NTP, -3);
    EXPECT_EQ(text(sett), "http:503,wtr:-100,ntp:-3");
}

TEST(ConnectLog, LongestTextFits)
{
    Settings sett;
    connect_log_push(sett, CONNECT_MQTT, -32768);
    connect_log_push(sett, CONNECT_MQTT, -32768);
    connect_log_push(sett, CONNECT_MQTT, -32768);
    EXPECT_EQ(text(sett), "mqtt:-32768,mqtt:-32768,mqtt:-32768");
}

TEST(ConnectLog, UnknownStageIsSkipped)
{
    Settings sett;
    sett.connect_log[2].stage = 200;
    sett.connect_log[2].code = 5;
    EXPECT_EQ(text(sett), "");
}
