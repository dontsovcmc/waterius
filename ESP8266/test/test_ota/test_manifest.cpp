#include <gtest/gtest.h>
#include "ota_manifest.h"

TEST(ParseManifest, ValidFullManifest)
{
    const char *json = R"({
        "version": "2.0.25",
        "firmware": {
            "url": "https://cloud.waterius.ru/ota/fw.bin",
            "size": 615432,
            "md5": "d41d8cd98f00b204e9800998ecf8427e"
        },
        "filesystem": {
            "url": "https://cloud.waterius.ru/ota/fs.bin",
            "size": 1048576,
            "md5": "098f6bcd4621d373cade4e832627b4f6"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_NONE);
    EXPECT_TRUE(m.has_firmware);
    EXPECT_TRUE(m.has_filesystem);
    EXPECT_STREQ(m.fw_url, "https://cloud.waterius.ru/ota/fw.bin");
    EXPECT_STREQ(m.fw_md5, "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(m.fw_size, 615432u);
    EXPECT_STREQ(m.fs_url, "https://cloud.waterius.ru/ota/fs.bin");
    EXPECT_STREQ(m.fs_md5, "098f6bcd4621d373cade4e832627b4f6");
    EXPECT_EQ(m.fs_size, 1048576u);
}

TEST(ParseManifest, FirmwareOnly)
{
    const char *json = R"({
        "firmware": {
            "url": "https://cloud.waterius.ru/ota/fw.bin",
            "size": 100,
            "md5": "abc123"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_NONE);
    EXPECT_TRUE(m.has_firmware);
    EXPECT_FALSE(m.has_filesystem);
}

TEST(ParseManifest, FilesystemOnly)
{
    const char *json = R"({
        "filesystem": {
            "url": "https://cloud.waterius.ru/ota/fs.bin",
            "size": 200,
            "md5": "def456"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_NONE);
    EXPECT_FALSE(m.has_firmware);
    EXPECT_TRUE(m.has_filesystem);
}

TEST(ParseManifest, EmptyJson)
{
    const char *json = "{}";
    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_PARSE);
}

TEST(ParseManifest, InvalidJson)
{
    const char *json = "{{not json";
    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_PARSE);
}

TEST(ParseManifest, NullInput)
{
    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(nullptr, 0, doc, m), OTA_ERR_PARSE);
}

TEST(ParseManifest, ZeroLength)
{
    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest("{}", 0, doc, m), OTA_ERR_PARSE);
}

TEST(ParseManifest, FirmwareMissingUrl)
{
    const char *json = R"({
        "firmware": {
            "size": 100,
            "md5": "abc123"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_PARSE);
}

TEST(ParseManifest, FirmwareMissingMd5)
{
    const char *json = R"({
        "firmware": {
            "url": "https://cloud.waterius.ru/ota/fw.bin",
            "size": 100
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_PARSE);
}

TEST(ParseManifest, HttpUrlInFirmware)
{
    const char *json = R"({
        "firmware": {
            "url": "http://cloud.waterius.ru/ota/fw.bin",
            "size": 100,
            "md5": "abc123"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_INVALID_URL);
}

TEST(ParseManifest, HttpUrlInFilesystem)
{
    const char *json = R"({
        "filesystem": {
            "url": "http://cloud.waterius.ru/ota/fs.bin",
            "size": 200,
            "md5": "def456"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_INVALID_URL);
}

TEST(ParseManifest, EmptyUrlInFirmware)
{
    const char *json = R"({
        "firmware": {
            "url": "",
            "size": 100,
            "md5": "abc123"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_INVALID_URL);
}

TEST(ParseManifest, OversizedJson)
{
    std::string json = R"({"firmware":{"url":"https://cloud.waterius.ru/ota/fw.bin","md5":"abc","size":1,"pad":")";
    json += std::string(OTA_MANIFEST_MAX_SIZE + 100, 'x');
    json += "\"}}";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json.c_str(), json.size(), doc, m), OTA_ERR_PARSE);
}

TEST(ParseManifest, SizeDefaultsToZero)
{
    const char *json = R"({
        "firmware": {
            "url": "https://cloud.waterius.ru/ota/fw.bin",
            "md5": "abc123"
        }
    })";

    JsonDocument doc;
    OtaManifest m;
    EXPECT_EQ(parse_manifest(json, strlen(json), doc, m), OTA_ERR_NONE);
    EXPECT_EQ(m.fw_size, 0u);
}
