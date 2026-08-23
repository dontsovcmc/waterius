#include <gtest/gtest.h>
#include "core/wifi.h"

/*
Канал и BSSID приходят из скрытых полей формы, то есть из списка сетей, а не
из рук пользователя. Ошибка здесь стоит дорого: с неверной парой первый
коннект уходит в пустоту, а на устройстве это лишние секунды работы радио.
*/

TEST(WifiChannel, RealChannelsPassThrough)
{
    EXPECT_EQ(parse_wifi_channel("1"), 1);
    EXPECT_EQ(parse_wifi_channel("6"), 6);
    EXPECT_EQ(parse_wifi_channel("13"), 13);
}

TEST(WifiChannel, OutOfRangeMeansUnknown)
{
    // Ноль — это "канал неизвестен, идём полным сканом", а не ошибка формы
    EXPECT_EQ(parse_wifi_channel("0"), 0);
    EXPECT_EQ(parse_wifi_channel("14"), 0);
    EXPECT_EQ(parse_wifi_channel("255"), 0);
    EXPECT_EQ(parse_wifi_channel("-1"), 0);
}

TEST(WifiChannel, EmptyAndGarbageMeanUnknown)
{
    // Пустым поле бывает при ручном вводе имени сети, мимо списка
    EXPECT_EQ(parse_wifi_channel(""), 0);
    EXPECT_EQ(parse_wifi_channel("abc"), 0);
    EXPECT_EQ(parse_wifi_channel(nullptr), 0);
}

TEST(ParseBssid, ColonSeparatedIsAccepted)
{
    uint8_t bssid[6] = {0};

    EXPECT_TRUE(parse_bssid("aa:bb:cc:dd:ee:ff", bssid));
    EXPECT_EQ(bssid[0], 0xAA);
    EXPECT_EQ(bssid[1], 0xBB);
    EXPECT_EQ(bssid[2], 0xCC);
    EXPECT_EQ(bssid[3], 0xDD);
    EXPECT_EQ(bssid[4], 0xEE);
    EXPECT_EQ(bssid[5], 0xFF);
}

TEST(ParseBssid, CaseAndSeparatorDoNotMatter)
{
    uint8_t lower[6] = {0}, upper[6] = {0}, dashed[6] = {0}, bare[6] = {0};

    EXPECT_TRUE(parse_bssid("0a:1b:2c:3d:4e:5f", lower));
    EXPECT_TRUE(parse_bssid("0A:1B:2C:3D:4E:5F", upper));
    EXPECT_TRUE(parse_bssid("0A-1B-2C-3D-4E-5F", dashed));
    EXPECT_TRUE(parse_bssid("0a1b2c3d4e5f", bare));

    EXPECT_EQ(memcmp(lower, upper, sizeof(lower)), 0);
    EXPECT_EQ(memcmp(lower, dashed, sizeof(lower)), 0);
    EXPECT_EQ(memcmp(lower, bare, sizeof(lower)), 0);
}

TEST(ParseBssid, ZeroAddressIsParsedButNotConsideredSet)
{
    // Роутер с таким MAC не бывает, а вот поле, забитое нулями, бывает
    uint8_t bssid[6] = {1, 1, 1, 1, 1, 1};

    EXPECT_TRUE(parse_bssid("00:00:00:00:00:00", bssid));
    EXPECT_FALSE(has_bssid(bssid));
}

TEST(ParseBssid, BrokenValueLeavesNoGarbage)
{
    // Половина разобранного адреса хуже, чем его отсутствие: с ней ЕСП
    // ушёл бы подключаться к несуществующей точке
    uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
    const uint8_t zero[6] = {0};

    EXPECT_FALSE(parse_bssid("aa:bb:cc", bssid));
    EXPECT_EQ(memcmp(bssid, zero, sizeof(zero)), 0);

    EXPECT_FALSE(parse_bssid("aa:bb:cc:dd:ee:ff:00", bssid));
    EXPECT_EQ(memcmp(bssid, zero, sizeof(zero)), 0);

    EXPECT_FALSE(parse_bssid("aa:bb:cc:dd:ee:gg", bssid));
    EXPECT_EQ(memcmp(bssid, zero, sizeof(zero)), 0);

    EXPECT_FALSE(parse_bssid("", bssid));
    EXPECT_FALSE(parse_bssid(nullptr, bssid));
    EXPECT_EQ(memcmp(bssid, zero, sizeof(zero)), 0);
}

TEST(HasBssid, AnyNonZeroByteCounts)
{
    const uint8_t empty[6] = {0};
    const uint8_t last_byte[6] = {0, 0, 0, 0, 0, 1};
    const uint8_t real[6] = {0x24, 0xA4, 0x3C, 0x11, 0x22, 0x33};

    EXPECT_FALSE(has_bssid(empty));
    EXPECT_TRUE(has_bssid(last_byte));
    EXPECT_TRUE(has_bssid(real));
}

TEST(ApChannel, ZeroFallsBackToFirstChannel)
{
    // Ноль лежит в настройках после неудачного коннекта и после смены сети.
    // Точка доступа портала на нуле может не подняться, а без неё
    // устройство не настроить вообще
    EXPECT_EQ(ap_channel(0), 1);
    EXPECT_EQ(ap_channel(14), 1);
    EXPECT_EQ(ap_channel(255), 1);
}

TEST(ApChannel, RealChannelIsKept)
{
    // Совпадение канала точки доступа с каналом роутера — единственный
    // способ не уронить телефон, когда ЕСП поднимет STA
    EXPECT_EQ(ap_channel(1), 1);
    EXPECT_EQ(ap_channel(6), 6);
    EXPECT_EQ(ap_channel(13), 13);
}
