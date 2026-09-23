#include <gtest/gtest.h>
#include "core/discovery.h"
#include "core/types.h"

namespace
{
    DiscoveryState base()
    {
        DiscoveryState s;
        s.firmware = "2.0.51";
        s.attiny_version = 41;
        s.model = 1;
        s.counter_type0 = CounterType::NAMUR;
        s.counter_type1 = CounterType::NAMUR;
        s.counter_name0 = CounterName::WATER_HOT;
        s.counter_name1 = CounterName::WATER_COLD;
        s.topic = "waterius-123";
        s.discovery_topic = "homeassistant";
        return s;
    }
}

TEST(DiscoverySignature, SameStateSameSignature)
{
    EXPECT_EQ(discovery_signature(base()), discovery_signature(base()));
}

TEST(DiscoverySignature, NeverZero)
{
    // Ноль в настройках значит "не публиковали" - совпадение с ним
    // навсегда отключило бы переотправку
    EXPECT_NE(discovery_signature(base()), 0u);

    DiscoveryState empty = {};
    EXPECT_NE(discovery_signature(empty), 0u);
}

TEST(DiscoverySignature, EveryFieldChangesIt)
{
    const uint32_t was = discovery_signature(base());

    DiscoveryState s = base();
    s.firmware = "2.0.52";
    EXPECT_NE(discovery_signature(s), was) << "версия ЕСП";

    s = base();
    s.attiny_version = 42;
    EXPECT_NE(discovery_signature(s), was) << "версия attiny";

    s = base();
    s.model = 0;
    EXPECT_NE(discovery_signature(s), was) << "модель";

    s = base();
    s.counter_type0 = CounterType::LEAKAGE;
    EXPECT_NE(discovery_signature(s), was) << "тип входа 0";

    s = base();
    s.counter_type1 = CounterType::NONE;
    EXPECT_NE(discovery_signature(s), was) << "тип входа 1";

    s = base();
    s.counter_name0 = CounterName::ELECTRO;
    EXPECT_NE(discovery_signature(s), was) << "ресурс 0";

    s = base();
    s.counter_name1 = CounterName::GAS;
    EXPECT_NE(discovery_signature(s), was) << "ресурс 1";

    s = base();
    s.topic = "waterius-124";
    EXPECT_NE(discovery_signature(s), was) << "топик";

    s = base();
    s.discovery_topic = "ha";
    EXPECT_NE(discovery_signature(s), was) << "топик автообнаружения";
}

TEST(DiscoverySignature, SwappedInputsDiffer)
{
    // Типы входов переставлены местами - сущности у входов другие
    DiscoveryState a = base();
    a.counter_type0 = CounterType::LEAKAGE;
    DiscoveryState b = base();
    b.counter_type1 = CounterType::LEAKAGE;

    EXPECT_NE(discovery_signature(a), discovery_signature(b));
}

TEST(DiscoverySignature, StringBoundaryMatters)
{
    DiscoveryState a = base();
    a.topic = "ab";
    a.discovery_topic = "c";
    DiscoveryState b = base();
    b.topic = "a";
    b.discovery_topic = "bc";

    EXPECT_NE(discovery_signature(a), discovery_signature(b));
}

namespace
{
    const ChannelEntity ALL[] = {
        ChannelEntity::INPUT_TYPE, ChannelEntity::WET, ChannelEntity::READINGS,
        ChannelEntity::SERIAL_NUMBER, ChannelEntity::FACTOR, ChannelEntity::RESOURCE,
        ChannelEntity::ALARM_CONFIG, ChannelEntity::ALARM_STATE,
    };
}

TEST(ChannelEntities, InputTypeIsAlwaysPublished)
{
    const uint8_t types[] = {CounterType::NAMUR, CounterType::ELECTRONIC,
                             CounterType::ELECTRONIC_HIGH, CounterType::LEAKAGE,
                             CounterType::LEAKAGE_NC, CounterType::NONE};
    for (uint8_t t : types)
        EXPECT_TRUE(channel_entity_wanted(t, ChannelEntity::INPUT_TYPE)) << (int)t;
}

TEST(ChannelEntities, MeterHasEverythingButWet)
{
    const uint8_t meters[] = {CounterType::NAMUR, CounterType::ELECTRONIC,
                              CounterType::ELECTRONIC_HIGH};
    for (uint8_t t : meters)
    {
        for (ChannelEntity e : ALL)
        {
            EXPECT_EQ(channel_entity_wanted(t, e), e != ChannelEntity::WET)
                << "тип " << (int)t << ", группа " << (int)e;
        }
    }
}

TEST(ChannelEntities, LeakSensorHasOnlyTypeAndWet)
{
    const uint8_t sensors[] = {CounterType::LEAKAGE, CounterType::LEAKAGE_NC};
    for (uint8_t t : sensors)
    {
        for (ChannelEntity e : ALL)
        {
            const bool want = e == ChannelEntity::INPUT_TYPE || e == ChannelEntity::WET;
            EXPECT_EQ(channel_entity_wanted(t, e), want)
                << "тип " << (int)t << ", группа " << (int)e;
        }
    }
}

TEST(ChannelEntities, DisabledInputHasOnlyType)
{
    for (ChannelEntity e : ALL)
    {
        EXPECT_EQ(channel_entity_wanted(CounterType::NONE, e), e == ChannelEntity::INPUT_TYPE)
            << "группа " << (int)e;
    }
}
