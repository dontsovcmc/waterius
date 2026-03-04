#include <gtest/gtest.h>
#include "ota_manifest.h"

TEST(IsValidOtaUrl, ValidHttps)
{
    EXPECT_TRUE(is_valid_ota_url("https://cloud.waterius.ru/ota/fw.bin"));
}

TEST(IsValidOtaUrl, HttpRejected)
{
    EXPECT_FALSE(is_valid_ota_url("http://cloud.waterius.ru/ota/fw.bin"));
}

TEST(IsValidOtaUrl, EmptyString)
{
    EXPECT_FALSE(is_valid_ota_url(""));
}

TEST(IsValidOtaUrl, NullPtr)
{
    EXPECT_FALSE(is_valid_ota_url(nullptr));
}

TEST(IsValidOtaUrl, FtpRejected)
{
    EXPECT_FALSE(is_valid_ota_url("ftp://server/file.bin"));
}

TEST(IsValidOtaUrl, HttpsSchemeOnly)
{
    EXPECT_FALSE(is_valid_ota_url("https://"));
}

TEST(IsValidOtaUrl, HttpsWithPath)
{
    EXPECT_TRUE(is_valid_ota_url("https://a.b/c"));
}
