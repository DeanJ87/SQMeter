#include <unity.h>

#include <ArduinoJson.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

#include "Config.h"

using namespace SQM;

// Settings saved by every earlier release load without losing anything that
// release stored (spec 011 FR-005 / SC-003). The fixtures in
// test/fixtures/config-releases are what each tagged release's own config
// code wrote for the same set-up device (WiFi, MQTT, auth, location, safety
// rules and calibration changed from the defaults).

namespace
{
    std::string readFile(const std::string &path)
    {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    void assertSame(JsonVariantConst stored, JsonVariantConst loaded, const std::string &path)
    {
        if (stored.is<JsonObjectConst>())
        {
            for (JsonPairConst field : stored.as<JsonObjectConst>())
                assertSame(field.value(), loaded[field.key()], path + "." + field.key().c_str());
            return;
        }
        if (stored.is<JsonArrayConst>())
            return; // lists (e.g. publish groups) are covered by their own tests
        const std::string message = "changed on load: " + path;
        if (stored.is<float>() && !stored.is<long long>())
        {
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1e-4F, stored.as<float>(), loaded.as<float>(), message.c_str());
            return;
        }
        TEST_ASSERT_EQUAL_STRING_MESSAGE(stored.as<std::string>().c_str(), loaded.as<std::string>().c_str(), message.c_str());
    }

    // Settings the current firmware deliberately renamed, migrated or no
    // longer stores; each is checked by its own test elsewhere.
    bool migrated(const std::string &path)
    {
        static const char *const PATHS[] = {
            ".alerts.armWithAlpaca", // spec 021: becomes alerts.sendMode (test_config_model)
            // Dropped in v0.2.0-beta.2: never set by the UI (always 0 / "UTC");
            // the time zone the device uses is ntp.timezone, which is kept.
            ".timezone",
            ".ntp.gmtOffsetSec",
            ".ntp.daylightOffsetSec",
        };
        for (const char *p : PATHS)
            if (path == p)
                return true;
        return false;
    }

    void assertKept(JsonObjectConst stored, JsonObjectConst loaded, const std::string &prefix = "")
    {
        for (JsonPairConst field : stored)
        {
            const std::string path = prefix + "." + field.key().c_str();
            if (migrated(path))
                continue;
            if (field.value().is<JsonObjectConst>())
                assertKept(field.value().as<JsonObjectConst>(), loaded[field.key()].as<JsonObjectConst>(), path);
            else
                assertSame(field.value(), loaded[field.key()], path);
        }
    }

    void loadRelease(const char *tag)
    {
        const std::string stored = readFile(std::string("test/fixtures/config-releases/") + tag + ".json");
        TEST_ASSERT_FALSE_MESSAGE(stored.empty(), tag);

        Config loaded = Config::createDefault();
        std::string reason;
        const bool ok = Config::applyJson(stored, loaded, false, &reason);
        TEST_ASSERT_TRUE_MESSAGE(ok, (std::string(tag) + " rejected: " + reason).c_str());

        DynamicJsonDocument before(16384);
        DynamicJsonDocument after(16384);
        TEST_ASSERT_FALSE(deserializeJson(before, stored));
        TEST_ASSERT_FALSE(deserializeJson(after, loaded.toJson(false)));
        assertKept(before.as<JsonObjectConst>(), after.as<JsonObjectConst>());

        // Spot checks that matter most: still on the network, still protected.
        TEST_ASSERT_EQUAL_STRING("HomeNet", loaded.wifi.ssid.c_str());
        TEST_ASSERT_EQUAL_STRING("secret-wifi", loaded.wifi.password.c_str());
        TEST_ASSERT_TRUE(loaded.auth.enabled);
        TEST_ASSERT_EQUAL_STRING("adminpass", loaded.auth.password.c_str());
        TEST_ASSERT_TRUE(loaded.mqtt.enabled);
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_v0_1_0() { loadRelease("v0.1.0"); }
void test_v0_1_3() { loadRelease("v0.1.3"); }
void test_v0_1_4_beta_1() { loadRelease("v0.1.4-beta.1"); }
void test_v0_2_0_beta_1() { loadRelease("v0.2.0-beta.1"); }
void test_v0_2_0_beta_2() { loadRelease("v0.2.0-beta.2"); }
void test_v0_2_0_beta_3() { loadRelease("v0.2.0-beta.3"); }

// Settings too big for their NVS entry are refused with a message that says
// what to do, rather than saved and lost on the next boot.
void test_oversized_alert_texts_are_refused_clearly()
{
    // Quotes are stored escaped (\"), so each field takes twice its length
    // in the stored JSON: within each field's limit, too big in total.
    std::string message;
    for (int i = 0; i < 240; i++)
        message += "\\\"";
    const std::string title(80, 'y');
    std::string events;
    for (const char *name : {"unsafe", "safe", "rain_started", "rain_stopped", "sensor_fault", "sensor_recovered", "dew_risk", "clear_sky", "clouded_over"})
    {
        events += events.empty() ? "" : ",";
        events += std::string("\"") + name + "\":{\"level\":2,\"title\":\"" + title + "\",\"message\":\"" + message + "\"}";
    }
    const Config base = Config::createDefault();
    std::string reason;
    const auto cfg = Config::fromJson("{\"alerts\":{\"events\":{" + events + "}}}", &base, &reason);
    TEST_ASSERT_FALSE(cfg.has_value());
    TEST_ASSERT_EQUAL_STRING("Alerts: the custom alert texts are too long in total - shorten some", reason.c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_oversized_alert_texts_are_refused_clearly);
    RUN_TEST(test_v0_1_0);
    RUN_TEST(test_v0_1_3);
    RUN_TEST(test_v0_1_4_beta_1);
    RUN_TEST(test_v0_2_0_beta_1);
    RUN_TEST(test_v0_2_0_beta_2);
    RUN_TEST(test_v0_2_0_beta_3);
    return UNITY_END();
}
