#include <gtest/gtest.h>
#include <string.h>
#include "core/timekeeping.h"

/*
Разбор ответа NTP, валидность времени и обход пула серверов.

Раньше это сидело внутри сетевых функций sync_time.cpp и не тестировалось
никак: чтобы добраться до арифметики, пришлось бы подменять UDP. После
выноса в ядро моки не нужны — на входе массив байтов, на выходе число.
*/

namespace
{
    const uint32_t SECS_1900_TO_1970 = 2208988800UL;

    // Собирает ответ NTP: время передачи в байтах 40..47
    struct Packet
    {
        uint8_t bytes[NTP_PACKET_SIZE] = {0};

        Packet(const uint32_t secs_since_1900, const uint32_t fraction = 0)
        {
            bytes[0] = 0b00100100;   // LI=0, версия 4, режим server
            bytes[40] = (secs_since_1900 >> 24) & 0xFF;
            bytes[41] = (secs_since_1900 >> 16) & 0xFF;
            bytes[42] = (secs_since_1900 >> 8) & 0xFF;
            bytes[43] = secs_since_1900 & 0xFF;
            bytes[44] = (fraction >> 24) & 0xFF;
            bytes[45] = (fraction >> 16) & 0xFF;
            bytes[46] = (fraction >> 8) & 0xFF;
            bytes[47] = fraction & 0xFF;
        }

        // Время unix -> время NTP
        static Packet from_unix(const uint32_t unix_secs, const uint32_t fraction = 0)
        {
            return Packet(unix_secs + SECS_1900_TO_1970, fraction);
        }

        uint64_t parse() const { return parse_ntp_packet(bytes, sizeof(bytes)); }
        uint32_t unix_secs() const { return (uint32_t)(parse() / NSEC); }
    };
}

// --- валидность времени ---

TEST(ValidTime, ThresholdIsExclusive)
{
    EXPECT_FALSE(is_valid_time((time_t)START_VALID_TIME));
    EXPECT_TRUE(is_valid_time((time_t)START_VALID_TIME + 1));
}

TEST(ValidTime, ZeroAndEpochAreInvalid)
{
    EXPECT_FALSE(is_valid_time(0));
    EXPECT_FALSE(is_valid_time(1));
}

TEST(ValidTime, RealTimestampIsValid)
{
    EXPECT_TRUE(is_valid_time(1755000000));   // ~2026
}

// --- какое время ставить на часы перед запросом ---

TEST(ClockBeforeSync, KeepsLastKnownGoodTime)
{
    // Главное свойство: если запрос не удастся, часы останутся на последнем
    // достоверном времени, а не в прошлом. Иначе получается now < last_send.
    EXPECT_EQ(clock_before_sync(1755000000), 1755000000);
}

TEST(ClockBeforeSync, NeverGoesBackwards)
{
    const time_t last = 1755000000;
    EXPECT_GE(clock_before_sync(last), last);
}

TEST(ClockBeforeSync, FreshDeviceFallsBackToThreshold)
{
    // last_send == 0 у нового устройства: сажать часы в 1970 незачем,
    // остаётся порог, и is_valid_time не даст испортить конфигурацию
    EXPECT_EQ(clock_before_sync(0), (time_t)START_VALID_TIME);
}

TEST(ClockBeforeSync, BogusOldTimeFallsBackToThreshold)
{
    EXPECT_EQ(clock_before_sync(1600000000), (time_t)START_VALID_TIME);   // 2020
}

// --- разбор ответа NTP ---

TEST(ParseNtp, RealisticPacket)
{
    Packet p = Packet::from_unix(1755000000);

    EXPECT_EQ(p.unix_secs(), 1755000000u);
}

TEST(ParseNtp, FractionBecomesNanoseconds)
{
    // Половина секунды: старший бит дробной части
    Packet p = Packet::from_unix(1755000000, 0x80000000);

    EXPECT_EQ(p.parse(), 1755000000ULL * NSEC + NSEC / 2);
}

TEST(ParseNtp, MaxFractionStaysBelowOneSecond)
{
    Packet p = Packet::from_unix(1755000000, 0xFFFFFFFF);

    EXPECT_LT(p.parse() - 1755000000ULL * NSEC, (uint64_t)NSEC);
}

TEST(ParseNtp, ShortPacketRejected)
{
    Packet p = Packet::from_unix(1755000000);

    EXPECT_EQ(parse_ntp_packet(p.bytes, NTP_PACKET_SIZE - 1), 0u);
}

TEST(ParseNtp, NullPacketRejected)
{
    EXPECT_EQ(parse_ntp_packet(nullptr, NTP_PACKET_SIZE), 0u);
}

TEST(ParseNtp, UnsyncedServerRejected)
{
    // Leap Indicator = 3: у сервера самого нет синхронизации
    Packet p = Packet::from_unix(1755000000);
    p.bytes[0] |= 0b11000000;

    EXPECT_EQ(p.parse(), 0u);
}

TEST(ParseNtp, ZeroTimestampRejected)
{
    Packet p(0);

    EXPECT_EQ(p.parse(), 0u);
}

TEST(ParseNtp, TimestampBeforeUnixEpochRejected)
{
    // Битый пакет или чужой UDP-ответ на наш порт: значение маленькое, но
    // не ноль. Вычитание 70 лет уводит его в переполнение uint64, и часы
    // уезжают в 2484 год — а оттуда значение попадает в last_send в EEPROM,
    // после чего now < last_send становится постоянным состоянием.
    EXPECT_EQ(Packet(1).parse(), 0u);
    EXPECT_EQ(Packet(100).parse(), 0u);
    EXPECT_EQ(Packet(SECS_1900_TO_1970 - 1).parse(), 0u);
}

TEST(ParseNtp, UnixEpochItselfRejected)
{
    // 1970-01-01 — не время, а признак сброшенных часов
    EXPECT_EQ(Packet(SECS_1900_TO_1970).parse(), 0u);
}

TEST(ParseNtp, MaxTimestampDoesNotOverflow)
{
    // Верхняя граница поля NTP: 2036 год, переполнения uint64 быть не должно
    Packet p(0xFFFFFFFF);

    EXPECT_EQ(p.unix_secs(), 0xFFFFFFFFu - SECS_1900_TO_1970);
}

// --- обход пула серверов ---

TEST(NtpPool, RotatesInCircle)
{
    EXPECT_EQ(next_ntp_server_id(0), 1);
    EXPECT_EQ(next_ntp_server_id(1), 2);
    EXPECT_EQ(next_ntp_server_id(2), 3);
    EXPECT_EQ(next_ntp_server_id(3), 0);
}

TEST(NtpPool, CoversWholePool)
{
    // Все четыре сервера пула должны быть достижимы: 3.ru.pool.ntp.org
    // не должен оказаться недосягаемым
    bool seen[NTP_POOL_SIZE] = {false};
    int id = 0;
    for (int i = 0; i < NTP_POOL_SIZE * 2; ++i)
    {
        seen[id] = true;
        id = next_ntp_server_id(id);
    }

    for (int i = 0; i < NTP_POOL_SIZE; ++i)
        EXPECT_TRUE(seen[i]) << "сервер " << i << " недостижим";
}

TEST(NtpPool, NegativeStartsFromFirst)
{
    EXPECT_EQ(next_ntp_server_id(-1), 0);
}
