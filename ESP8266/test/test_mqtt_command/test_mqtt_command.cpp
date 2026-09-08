#include <gtest/gtest.h>
#include <string>
#include "core/mqtt_command.h"

/*
Тесты фиксируют, что устройство считает командой. Два свойства главные, и оба
про то, чего командой считать нельзя: топик показаний (#422) и пустое значение
(#421). Обратное устройство делает с собственными сообщениями, вернувшимися к
нему по подписке на своё же дерево.
*/

namespace
{
    const size_t ANY_PAYLOAD = 2;

    bool is_command(const char *topic, const size_t payload_len = ANY_PAYLOAD)
    {
        size_t pos = 0;
        size_t len = 0;
        return parse_mqtt_command(topic, payload_len, &pos, &len);
    }

    // Имя параметра или пустая строка, если это не команда: пустое имя
    // командой не считается, так что путаницы нет
    std::string param(const char *topic, const size_t payload_len = ANY_PAYLOAD)
    {
        size_t pos = 0;
        size_t len = 0;
        if (!parse_mqtt_command(topic, payload_len, &pos, &len))
        {
            return "";
        }
        return std::string(topic + pos, len);
    }
}

TEST(MqttCommand, SetTopicGivesParamName)
{
    EXPECT_EQ(param("waterius/period_min/set"), "period_min");
    EXPECT_EQ(param("waterius/vac/set"), "vac");
    EXPECT_EQ(param("waterius/kitchen/af1/set"), "af1");
}

// #421: пустое сообщение в топике команды - снятие retain, а не значение
TEST(MqttCommand, EmptyPayloadIsNotCommand)
{
    EXPECT_FALSE(is_command("waterius/period_min/set", 0));
    EXPECT_FALSE(is_command("waterius/vac/set", 0));
}

// #422: показания уходят в то же дерево, на которое устройство подписано
TEST(MqttCommand, DataTopicIsNotCommand)
{
    EXPECT_FALSE(is_command("waterius"));
    EXPECT_FALSE(is_command("waterius/f1"));
    EXPECT_FALSE(is_command("waterius/adc0"));
}

TEST(MqttCommand, SetInTheMiddleIsNotCommand)
{
    EXPECT_FALSE(is_command("waterius/set/period_min"));
    EXPECT_FALSE(is_command("waterius/settings"));
}

TEST(MqttCommand, TopicWithoutParamNameIsNotCommand)
{
    EXPECT_FALSE(is_command("/set"));
    EXPECT_FALSE(is_command("waterius//set"));
    EXPECT_FALSE(is_command("set"));
}
