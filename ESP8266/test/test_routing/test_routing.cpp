#include <gtest/gtest.h>
#include <string.h>
#include "core/routing.h"

/*
Тесты фиксируют, при каких настройках какой транспорт считается настроенным.
Главное свойство: транспорты независимы. Выключенная передача на waterius.ru
не должна выключать MQTT (#320).
*/

namespace
{
    void set(char *field, const char *value)
    {
        strcpy(field, value);
    }

    // Все три транспорта настроены и включены
    Settings all_configured()
    {
        Settings sett;
        sett.waterius_on = 1;
        set(sett.waterius_host, "https://cloud.waterius.ru");
        set(sett.waterius_key, "key");

        sett.http_on = 1;
        set(sett.http_url, "http://192.168.0.10/data");

        sett.mqtt_on = 1;
        set(sett.mqtt_host, "192.168.0.20");
        sett.mqtt_auto_discovery = 1;

        return sett;
    }
}

TEST(Routing, AllConfiguredMeansAllEnabled)
{
    Settings sett = all_configured();

    EXPECT_TRUE(is_waterius_site(sett));
    EXPECT_TRUE(is_http(sett));
    EXPECT_TRUE(is_mqtt(sett));
    EXPECT_TRUE(is_ha(sett));
}

TEST(Routing, NothingConfiguredMeansNothingEnabled)
{
    Settings sett;   // значения по умолчанию: адреса пустые
    sett.waterius_on = 0;
    sett.http_on = 0;
    sett.mqtt_on = 0;

    EXPECT_FALSE(is_waterius_site(sett));
    EXPECT_FALSE(is_http(sett));
    EXPECT_FALSE(is_mqtt(sett));
    EXPECT_FALSE(is_ha(sett));
}

// --- независимость транспортов ---

TEST(Routing, WateriusOffKeepsMqttAndHttp)
{
    // #320: пользователь отключил передачу на waterius.ru — остальные
    // транспорты обязаны продолжать работать
    Settings sett = all_configured();
    sett.waterius_on = 0;

    EXPECT_FALSE(is_waterius_site(sett));
    EXPECT_TRUE(is_mqtt(sett));
    EXPECT_TRUE(is_http(sett));
    EXPECT_TRUE(is_ha(sett));
}

TEST(Routing, MqttOffKeepsWateriusAndHttp)
{
    Settings sett = all_configured();
    sett.mqtt_on = 0;

    EXPECT_FALSE(is_mqtt(sett));
    EXPECT_TRUE(is_waterius_site(sett));
    EXPECT_TRUE(is_http(sett));
}

TEST(Routing, HttpOffKeepsWateriusAndMqtt)
{
    Settings sett = all_configured();
    sett.http_on = 0;

    EXPECT_FALSE(is_http(sett));
    EXPECT_TRUE(is_waterius_site(sett));
    EXPECT_TRUE(is_mqtt(sett));
}

// --- флажка мало, нужен адрес ---

TEST(Routing, WateriusNeedsHost)
{
    Settings sett = all_configured();
    sett.waterius_host[0] = 0;

    EXPECT_FALSE(is_waterius_site(sett));
}

TEST(Routing, WateriusNeedsKey)
{
    // Без ключа сервер не примет данные, отправлять бессмысленно
    Settings sett = all_configured();
    sett.waterius_key[0] = 0;

    EXPECT_FALSE(is_waterius_site(sett));
}

TEST(Routing, HttpNeedsUrl)
{
    Settings sett = all_configured();
    sett.http_url[0] = 0;

    EXPECT_FALSE(is_http(sett));
}

TEST(Routing, MqttNeedsHost)
{
    Settings sett = all_configured();
    sett.mqtt_host[0] = 0;

    EXPECT_FALSE(is_mqtt(sett));
    EXPECT_FALSE(is_ha(sett));   // HA живёт поверх MQTT
}

TEST(Routing, MqttPortIsNotRequired)
{
    // Порт по умолчанию уже проставлен, отдельной проверки нет
    Settings sett = all_configured();
    sett.mqtt_port = 0;

    EXPECT_TRUE(is_mqtt(sett));
}

// --- Home Assistant поверх MQTT ---

TEST(Routing, HaRequiresMqttEnabled)
{
    Settings sett = all_configured();
    sett.mqtt_on = 0;

    EXPECT_FALSE(is_ha(sett));
}

TEST(Routing, HaRequiresAutoDiscovery)
{
    Settings sett = all_configured();
    sett.mqtt_auto_discovery = 0;

    EXPECT_TRUE(is_mqtt(sett));
    EXPECT_FALSE(is_ha(sett));
}

// --- сеть ---

TEST(Routing, DhcpIsOnUnlessTurnedOff)
{
    Settings sett;
    sett.dhcp_off = 0;
    EXPECT_TRUE(is_dhcp(sett));

    sett.dhcp_off = 1;
    EXPECT_FALSE(is_dhcp(sett));
}
