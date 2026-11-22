#include <unity.h>

#include <string>

#include "Config.h"

// Rain needs cloud settings (specs/036-rain-needs-cloud, contracts/interfaces.md).

using namespace SQM;

namespace
{
    std::string rainReject(const std::string &json)
    {
        std::string error;
        const Config base = Config::createDefault();
        const auto cfg = Config::fromJson(json, &base, &error);
        return cfg ? std::string() : error;
    }

    const char *const LEVEL_MSG = "Safety: cloud to confirm rain must be 1 to 100%";
} // namespace

void test_rain_needs_cloud_defaults_off_at_ten_percent()
{
    const Config c = Config::createDefault();
    TEST_ASSERT_FALSE(c.alpaca.rainNeedsCloud);
    TEST_ASSERT_EQUAL_UINT8(10, c.alpaca.rainNeedsCloudPercent);
}

void test_rain_needs_cloud_round_trips()
{
    Config c = Config::createDefault();
    c.alpaca.rainNeedsCloud = true;
    c.alpaca.rainNeedsCloudPercent = 35;
    std::string error;
    const auto loaded = Config::fromJson(c.toJson(), nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    TEST_ASSERT_TRUE(loaded->alpaca.rainNeedsCloud);
    TEST_ASSERT_EQUAL_UINT8(35, loaded->alpaca.rainNeedsCloudPercent);
}

void test_rain_needs_cloud_missing_from_older_config_loads_off()
{
    // An older release's alpaca object has neither key (FR-014).
    std::string error;
    const auto loaded = Config::fromJson("{\"alpaca\":{\"enabled\":true,\"rainUnsafeEnabled\":true}}", nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    TEST_ASSERT_FALSE(loaded->alpaca.rainNeedsCloud);
    TEST_ASSERT_EQUAL_UINT8(10, loaded->alpaca.rainNeedsCloudPercent);
    TEST_ASSERT_TRUE(loaded->alpaca.rainUnsafeEnabled);
}

void test_rain_needs_cloud_level_range()
{
    TEST_ASSERT_EQUAL_STRING("", rainReject("{\"alpaca\":{\"rainNeedsCloudPercent\":1}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", rainReject("{\"alpaca\":{\"rainNeedsCloudPercent\":100}}").c_str());
    TEST_ASSERT_EQUAL_STRING(LEVEL_MSG, rainReject("{\"alpaca\":{\"rainNeedsCloudPercent\":0}}").c_str());
    TEST_ASSERT_EQUAL_STRING(LEVEL_MSG, rainReject("{\"alpaca\":{\"rainNeedsCloudPercent\":101}}").c_str());
    TEST_ASSERT_TRUE(rainReject("{\"alpaca\":{\"rainNeedsCloud\":\"yes\"}}").size() > 0);
    TEST_ASSERT_TRUE(rainReject("{\"alpaca\":{\"rainNeedsCloudPercent\":10.5}}").size() > 0);
}
