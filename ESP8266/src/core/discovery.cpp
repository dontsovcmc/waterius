#include "discovery.h"
#include "types.h"

namespace
{
    // FNV-1a, 32 бита
    const uint32_t FNV_OFFSET = 2166136261u;
    const uint32_t FNV_PRIME = 16777619u;

    uint32_t mix_byte(uint32_t hash, uint8_t byte)
    {
        return (hash ^ byte) * FNV_PRIME;
    }

    // С завершающим нулём: иначе "ab"+"c" и "a"+"bc" дали бы одно и то же
    uint32_t mix_string(uint32_t hash, const char *s)
    {
        if (s != nullptr)
        {
            for (; *s; ++s)
                hash = mix_byte(hash, (uint8_t)*s);
        }
        return mix_byte(hash, 0);
    }
}

uint32_t discovery_signature(const DiscoveryState &state)
{
    uint32_t hash = FNV_OFFSET;

    hash = mix_string(hash, state.firmware);
    hash = mix_byte(hash, state.attiny_version);
    hash = mix_byte(hash, state.model);
    hash = mix_byte(hash, state.counter_type0);
    hash = mix_byte(hash, state.counter_type1);
    hash = mix_byte(hash, state.counter_name0);
    hash = mix_byte(hash, state.counter_name1);
    hash = mix_string(hash, state.topic);
    hash = mix_string(hash, state.discovery_topic);

    return hash == 0 ? 1 : hash;
}

bool channel_entity_wanted(uint8_t counter_type, ChannelEntity entity)
{
    switch (entity)
    {
    case ChannelEntity::INPUT_TYPE:
        // Иначе включить выключенный вход из HA было бы нечем
        return true;
    case ChannelEntity::WET:
        return counter_type == CounterType::LEAKAGE || counter_type == CounterType::LEAKAGE_NC;
    default:
        return counts_impulses(counter_type);
    }
}
