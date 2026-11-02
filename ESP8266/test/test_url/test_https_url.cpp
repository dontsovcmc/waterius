#include <gtest/gtest.h>
#include "core/url.h"

TEST(HttpsUrl, RequiresHttpsAndHost)
{
    EXPECT_TRUE(is_https_url("https://cloud.waterius.ru/ota/fw.bin"));
    EXPECT_TRUE(is_https_url("HTTPS://example.com:8443/fw.bin"));
    EXPECT_FALSE(is_https_url(nullptr));
    EXPECT_FALSE(is_https_url(""));
    EXPECT_FALSE(is_https_url("http://example.com/fw.bin"));
    EXPECT_FALSE(is_https_url("example.com/fw.bin"));
    EXPECT_FALSE(is_https_url("https:///fw.bin"));
    EXPECT_FALSE(is_https_url("https://:443/fw.bin"));
    EXPECT_FALSE(is_https_url("https://?url=example.com"));
}
