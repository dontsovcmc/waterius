#include <gtest/gtest.h>
#include "core/url.h"

/*
#330: пользователи вводят адрес брокера как URL — со схемой, портом, путём.
PubSubClient хочет голое имя хоста.
*/

namespace
{
    // Удобная обёртка: адрес + результат разбора в одном месте
    struct Parsed
    {
        char host[HOST_LEN] = {0};
        ParamError err = PARAM_OK;

        explicit Parsed(const char *value)
        {
            err = parse_broker_host(host, sizeof(host), value);
        }
    };
}

// --- обычный ввод ---

TEST(BrokerHost, PlainHostPassesThrough)
{
    Parsed p("broker.example.com");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "broker.example.com");
}

TEST(BrokerHost, IpAddressPassesThrough)
{
    Parsed p("192.168.0.20");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "192.168.0.20");
}

TEST(BrokerHost, SurroundingSpacesAreTrimmed)
{
    Parsed p("  broker.local  ");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "broker.local");
}

// --- схемы открытого TCP: снимаем ---

TEST(BrokerHost, MqttSchemeIsStripped)
{
    Parsed p("mqtt://broker.example.com");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "broker.example.com");
}

TEST(BrokerHost, TcpSchemeIsStripped)
{
    Parsed p("tcp://192.168.0.20");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "192.168.0.20");
}

TEST(BrokerHost, HttpSchemeIsStripped)
{
    Parsed p("http://broker.local");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "broker.local");
}

TEST(BrokerHost, SchemeCaseDoesNotMatter)
{
    EXPECT_STREQ(Parsed("MQTT://broker.local").host, "broker.local");
    EXPECT_STREQ(Parsed("Mqtt://broker.local").host, "broker.local");
    EXPECT_STREQ(Parsed("HTTP://broker.local").host, "broker.local");
}

TEST(BrokerHost, SchemeWithSpacesAround)
{
    Parsed p("  mqtt://broker.local  ");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "broker.local");
}

// --- шифрование: отвергаем, а не понижаем молча ---

TEST(BrokerHost, EncryptedSchemesAreRejected)
{
    // Прошивка подключается через WiFiClient без TLS. Снять схему молча
    // значило бы отправить логин и пароль открытым текстом.
    EXPECT_EQ(Parsed("mqtts://broker.example.com").err, PARAM_ERR_TLS);
    EXPECT_EQ(Parsed("ssl://broker.example.com").err, PARAM_ERR_TLS);
    EXPECT_EQ(Parsed("tls://broker.example.com").err, PARAM_ERR_TLS);
    EXPECT_EQ(Parsed("wss://broker.example.com").err, PARAM_ERR_TLS);
    EXPECT_EQ(Parsed("https://broker.example.com").err, PARAM_ERR_TLS);
}

TEST(BrokerHost, EncryptedSchemeCaseDoesNotMatter)
{
    EXPECT_EQ(Parsed("MQTTS://broker.local").err, PARAM_ERR_TLS);
}

TEST(BrokerHost, RejectedHostIsNotWritten)
{
    char host[HOST_LEN] = "old.broker";

    EXPECT_EQ(parse_broker_host(host, sizeof(host), "mqtts://new.broker"), PARAM_ERR_TLS);
    EXPECT_STREQ(host, "old.broker");
}

// --- неизвестные схемы ---

TEST(BrokerHost, UnknownSchemeIsInvalidValue)
{
    // ws:// — вебсокет, его PubSubClient поверх голого TCP не говорит
    EXPECT_EQ(Parsed("ws://broker.local").err, PARAM_ERR_VALUE);
    EXPECT_EQ(Parsed("foo://broker.local").err, PARAM_ERR_VALUE);
}

// --- порт: отвергаем, потому что молчаливое игнорирование уводит не туда ---

TEST(BrokerHost, PortInHostIsRejected)
{
    EXPECT_EQ(Parsed("broker.example.com:1883").err, PARAM_ERR_PORT_IN_HOST);
    EXPECT_EQ(Parsed("mqtt://broker.example.com:1883").err, PARAM_ERR_PORT_IN_HOST);
    EXPECT_EQ(Parsed("192.168.0.20:8883").err, PARAM_ERR_PORT_IN_HOST);
}

TEST(BrokerHost, IpV6LiteralIsRejectedAsPort)
{
    // IPv6 стек здесь всё равно не используется, литерал отсекается
    // проверкой на двоеточие
    EXPECT_EQ(Parsed("[fe80::1]").err, PARAM_ERR_PORT_IN_HOST);
}

// --- путь: отбрасываем, он ничего не решает ---

TEST(BrokerHost, TrailingSlashIsDropped)
{
    Parsed p("mqtt://broker.example.com/");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "broker.example.com");
}

TEST(BrokerHost, PathIsDropped)
{
    // Для MQTT поверх TCP путь не значит ничего и не может увести
    // подключение в другое место — в отличие от порта
    Parsed p("mqtt://broker.example.com/mqtt");

    EXPECT_EQ(p.err, PARAM_OK);
    EXPECT_STREQ(p.host, "broker.example.com");
}

TEST(BrokerHost, PortIsCheckedAfterPathIsDropped)
{
    EXPECT_EQ(Parsed("mqtt://broker.local:1883/mqtt").err, PARAM_ERR_PORT_IN_HOST);
}

// --- вырожденные случаи ---

TEST(BrokerHost, EmptyValueIsEmptyError)
{
    EXPECT_EQ(Parsed("").err, PARAM_ERR_EMPTY);
}

TEST(BrokerHost, SchemeWithoutHostIsEmptyError)
{
    EXPECT_EQ(Parsed("mqtt://").err, PARAM_ERR_EMPTY);
}

TEST(BrokerHost, TooLongHostIsLengthError)
{
    char host[8] = "keep";
    EXPECT_EQ(parse_broker_host(host, sizeof(host), "mqtt://broker.example.com"), PARAM_ERR_LENGTH);
    EXPECT_STREQ(host, "keep");
}

TEST(BrokerHost, ErrorCodesMatchWebInterface)
{
    // Коды разбираются в data/static/strings.js, менять нельзя
    EXPECT_EQ(PARAM_ERR_TLS, 20);
    EXPECT_EQ(PARAM_ERR_PORT_IN_HOST, 21);
}
