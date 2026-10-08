#include <unity.h>

#include <ArduinoJson.h>

#include "Config.h"

using namespace SQM;

namespace
{
    std::string rejectReason(const std::string &json)
    {
        const Config base = Config::createDefault();
        std::string error;
        const auto cfg = Config::fromJson(json, &base, &error);
        return cfg ? std::string() : error;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_defaults_are_valid_and_round_trip()
{
    const Config defaults = Config::createDefault();
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(defaults.validate(&error), error.c_str());
    TEST_ASSERT_EQUAL_STRING("sqmeter", defaults.wifi.hostname.c_str());
    TEST_ASSERT_TRUE(defaults.wifi.mdns);
    TEST_ASSERT_EQUAL_STRING("sqmeter", defaults.mqtt.topic.c_str());

    // Loading normalises (e.g. the backup time source when GPS is off);
    // after that, a save/load round trip changes nothing.
    const auto loaded = Config::fromJson(defaults.toJson());
    TEST_ASSERT_TRUE(loaded.has_value());
    const auto again = Config::fromJson(loaded->toJson());
    TEST_ASSERT_TRUE(again.has_value());
    TEST_ASSERT_EQUAL_STRING(loaded->toJson().c_str(), again->toJson().c_str());
}

void test_secrets_redacted_and_kept()
{
    Config cfg = Config::createDefault();
    cfg.wifi.ssid = "Home";
    cfg.wifi.password = "hunter22";
    const std::string redacted = cfg.toJson(true);
    TEST_ASSERT_TRUE(redacted.find("hunter22") == std::string::npos);
    TEST_ASSERT_TRUE(redacted.find("********") != std::string::npos);

    // Sending the mask back keeps the stored password.
    const auto updated = Config::fromJson(redacted, &cfg);
    TEST_ASSERT_TRUE(updated.has_value());
    TEST_ASSERT_EQUAL_STRING("hunter22", updated->wifi.password.c_str());
}

void test_partial_json_merges_onto_base()
{
    Config base = Config::createDefault();
    base.deviceName = "Observatory";
    const auto cfg = Config::fromJson("{\"gps\":{\"enabled\":true}}", &base);
    TEST_ASSERT_TRUE(cfg.has_value());
    TEST_ASSERT_TRUE(cfg->gps.enabled);
    TEST_ASSERT_EQUAL_STRING("Observatory", cfg->deviceName.c_str());
}

void test_rejections_say_why()
{
    TEST_ASSERT_EQUAL_STRING("Device name is required", rejectReason("{\"deviceName\":\"\"}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"wifi\":{\"hostname\":\"-bad\"}}").find("Hostname") == 0);
    TEST_ASSERT_TRUE(rejectReason("{\"mqtt\":{\"enabled\":true,\"broker\":\"b\",\"topic\":\"a/#\"}}").find("MQTT topic") == 0);
    TEST_ASSERT_TRUE(rejectReason("{\"sensor\":{\"readIntervalMs\":50}}").size() > 0);
    TEST_ASSERT_TRUE(rejectReason("{not json").find("couldn't be read") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"sensor\":{\"readIntervalMs\":100}}").c_str());
}

// Constraints (specs/020-settings-dependencies): combinations that can never
// work stay rejected with the device's message - D-27 time sources, D-33
// MQTT broker and topic, D-34 HTTP auth password. The web UI shows the same
// messages before saving (web/src/__tests__/configSchema.test.ts).
void test_constraints_rejected_with_messages()
{
    struct
    {
        const char *json;
        const char *message;
    } cases[] = {
        {R"({"ntp":{"enabled":false},"gps":{"enabled":false}})", "At least one time source must be enabled"},                      // D-27
        {R"({"mqtt":{"enabled":true,"broker":"","topic":"sqmeter"}})", "MQTT broker and topic are required when MQTT is enabled"}, // D-33
        {R"({"auth":{"enabled":true,"username":"admin","password":""}})", "HTTP auth password is required when auth is enabled"},  // D-34
    };
    for (const auto &c : cases)
    {
        std::string error;
        TEST_ASSERT_FALSE_MESSAGE(Config::fromJson(c.json, nullptr, &error).has_value(), c.json);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c.message, error.c_str(), c.json);
    }
}

// D-32 is a dependency, not a constraint: command-line uploads without a
// password are kept (devices in the field store this) and reported inactive.
void test_ota_without_password_is_kept()
{
    auto cfg = Config::fromJson(R"({"ota":{"enabled":true,"password":""}})");
    TEST_ASSERT_TRUE(cfg.has_value());
    TEST_ASSERT_TRUE(cfg->ota.enabled);
}

void test_read_interval_boundaries()
{
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"sensor\":{\"readIntervalMs\":3600000}}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"sensor\":{\"readIntervalMs\":99}}").size() > 0);
    TEST_ASSERT_TRUE(rejectReason("{\"sensor\":{\"readIntervalMs\":3600001}}").size() > 0);
}

void test_mqtt_interval_and_hostname_boundaries()
{
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"mqtt\":{\"publishIntervalMs\":86400000}}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"mqtt\":{\"publishIntervalMs\":86400001}}").size() > 0);
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"wifi\":{\"hostname\":\"a-32-character-hostname-ok-12345\"}}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"wifi\":{\"hostname\":\"a-33-character-hostname-ok-123456\"}}").size() > 0);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_defaults_are_valid_and_round_trip);
    RUN_TEST(test_secrets_redacted_and_kept);
    RUN_TEST(test_partial_json_merges_onto_base);
    RUN_TEST(test_rejections_say_why);
    RUN_TEST(test_read_interval_boundaries);
    RUN_TEST(test_constraints_rejected_with_messages);
    RUN_TEST(test_ota_without_password_is_kept);
    RUN_TEST(test_mqtt_interval_and_hostname_boundaries);
    return UNITY_END();
}
