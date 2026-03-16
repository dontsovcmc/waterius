#include <gtest/gtest.h>
#include "ota_parse.h"

// Полный OTA: firmware + filesystem
TEST(ParseOtaParams, FirmwareAndFilesystem)
{
    JsonDocument doc;
    doc["firmware"]["url"] = "https://cloud.waterius.ru/ota/fw.bin";
    doc["firmware"]["md5"] = "abc123";
    doc["firmware"]["size"] = 637200;
    doc["filesystem"]["url"] = "https://cloud.waterius.ru/ota/fs.bin";
    doc["filesystem"]["md5"] = "def456";
    doc["filesystem"]["size"] = 1024000;

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_NONE);
    EXPECT_TRUE(p.has_firmware);
    EXPECT_TRUE(p.has_filesystem);
    EXPECT_STREQ(p.fw_url, "https://cloud.waterius.ru/ota/fw.bin");
    EXPECT_STREQ(p.fw_md5, "abc123");
    EXPECT_EQ(p.fw_size, 637200u);
    EXPECT_STREQ(p.fs_url, "https://cloud.waterius.ru/ota/fs.bin");
    EXPECT_STREQ(p.fs_md5, "def456");
    EXPECT_EQ(p.fs_size, 1024000u);
}

// Только firmware
TEST(ParseOtaParams, FirmwareOnly)
{
    JsonDocument doc;
    doc["firmware"]["url"] = "https://example.com/fw.bin";
    doc["firmware"]["md5"] = "aaa";
    doc["firmware"]["size"] = 100;

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_NONE);
    EXPECT_TRUE(p.has_firmware);
    EXPECT_FALSE(p.has_filesystem);
    EXPECT_STREQ(p.fw_url, "https://example.com/fw.bin");
}

// Только filesystem
TEST(ParseOtaParams, FilesystemOnly)
{
    JsonDocument doc;
    doc["filesystem"]["url"] = "https://example.com/fs.bin";
    doc["filesystem"]["md5"] = "bbb";
    doc["filesystem"]["size"] = 200;

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_NONE);
    EXPECT_FALSE(p.has_firmware);
    EXPECT_TRUE(p.has_filesystem);
    EXPECT_STREQ(p.fs_url, "https://example.com/fs.bin");
}

// Пустой объект — ошибка
TEST(ParseOtaParams, EmptyObject)
{
    JsonDocument doc;
    doc.to<JsonObject>();

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_PARSE);
}

// firmware без url — ошибка
TEST(ParseOtaParams, FirmwareMissingUrl)
{
    JsonDocument doc;
    doc["firmware"]["md5"] = "abc";

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_PARSE);
}

// firmware без md5 — ошибка
TEST(ParseOtaParams, FirmwareMissingMd5)
{
    JsonDocument doc;
    doc["firmware"]["url"] = "https://example.com/fw.bin";

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_PARSE);
}

// filesystem без url — ошибка
TEST(ParseOtaParams, FilesystemMissingUrl)
{
    JsonDocument doc;
    doc["filesystem"]["md5"] = "abc";

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_PARSE);
}

// filesystem без md5 — ошибка
TEST(ParseOtaParams, FilesystemMissingMd5)
{
    JsonDocument doc;
    doc["filesystem"]["url"] = "https://example.com/fs.bin";

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_PARSE);
}

// size не указан — должен быть 0
TEST(ParseOtaParams, SizeDefaultsToZero)
{
    JsonDocument doc;
    doc["firmware"]["url"] = "https://example.com/fw.bin";
    doc["firmware"]["md5"] = "abc";

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_NONE);
    EXPECT_EQ(p.fw_size, 0u);
}

// firmware — не объект (строка) — ошибка
TEST(ParseOtaParams, FirmwareNotObject)
{
    JsonDocument doc;
    doc["firmware"] = "not_an_object";

    OtaParams p = parse_ota_params(doc.as<JsonObject>());

    EXPECT_EQ(p.error, OTA_ERR_PARSE);
}

// Коды ошибок — проверяем значения
TEST(OtaErrorCodes, Values)
{
    EXPECT_EQ(OTA_ERR_NONE, 0);
    EXPECT_EQ(OTA_ERR_PARSE, 1);
    EXPECT_EQ(OTA_ERR_FS_UPDATE, 2);
    EXPECT_EQ(OTA_ERR_FW_UPDATE, 3);
    EXPECT_EQ(OTA_ERR_LOW_BATTERY, 4);
}
