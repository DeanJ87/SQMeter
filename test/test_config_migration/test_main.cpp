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

void test_v0_1_0()
{
    loadRelease("v0.1.0");
}
void test_v0_1_3()
{
    loadRelease("v0.1.3");
}
void test_v0_1_4_beta_1()
{
    loadRelease("v0.1.4-beta.1");
}
void test_v0_2_0_beta_1()
{
    loadRelease("v0.2.0-beta.1");
}
void test_v0_2_0_beta_2()
{
    loadRelease("v0.2.0-beta.2");
}
void test_v0_2_0_beta_3()
{
    loadRelease("v0.2.0-beta.3");
}

// Spec 030 (FR-014): settings from before the second sky sensor load with
// the new keys at their defaults, and one sky sensor stays authoritative.
void test_pre_030_config_gets_sky_defaults()
{
    for (const char *tag : {"v0.1.0", "v0.2.0-beta.3"})
    {
        const std::string stored = readFile(std::string("test/fixtures/config-releases/") + tag + ".json");
        TEST_ASSERT_TRUE(stored.find("i2c2SDA") == std::string::npos);
        Config loaded = Config::createDefault();
        std::string reason;
        TEST_ASSERT_TRUE_MESSAGE(Config::applyJson(stored, loaded, false, &reason), reason.c_str());
        TEST_ASSERT_EQUAL_UINT8(255, loaded.sensor.i2c2SDA);
        TEST_ASSERT_EQUAL_UINT8(255, loaded.sensor.i2c2SCL);
        TEST_ASSERT_TRUE(loaded.sky.sqmEnabled);
        TEST_ASSERT_FALSE(loaded.sky.sqmlEnabled);
        TEST_ASSERT_TRUE(loaded.sky.authoritative == SkyRole::Sqm);
        TEST_ASSERT_TRUE(loaded.sky.showOther);
        TEST_ASSERT_EQUAL_UINT16(90, loaded.skyAveragingL.windowSeconds);
        TEST_ASSERT_FALSE(loaded.skyCalibrationL.enabled);
        TEST_ASSERT_EQUAL_FLOAT(0.0F, loaded.skyCalibrationL.sqmOffset);
    }
}

void test_v0_3_3_beta_1()
{
    loadRelease("v0.3.3-beta.1");
}

// Spec 031 (FR-017): settings from before the display load with every value
// kept (loadRelease) and the display off at its defaults.
void test_pre_031_config_gets_display_defaults()
{
    for (const char *tag : {"v0.1.0", "v0.3.3-beta.1"})
    {
        const std::string stored = readFile(std::string("test/fixtures/config-releases/") + tag + ".json");
        TEST_ASSERT_TRUE(stored.find("\"display\"") == std::string::npos);
        Config loaded = Config::createDefault();
        std::string reason;
        TEST_ASSERT_TRUE_MESSAGE(Config::applyJson(stored, loaded, false, &reason), reason.c_str());
        TEST_ASSERT_FALSE(loaded.display.enabled);
        TEST_ASSERT_EQUAL_UINT8(1, loaded.display.pageCount);
        TEST_ASSERT_TRUE(loaded.display.pages[0] == Display::Page::Sky);
        TEST_ASSERT_EQUAL_UINT8(5, loaded.display.cycleSeconds);
        TEST_ASSERT_TRUE(loaded.display.brightness == Display::Brightness::Medium);
        TEST_ASSERT_EQUAL_UINT16(60, loaded.display.timeoutSeconds);
        TEST_ASSERT_TRUE(loaded.display.wakeOnBoot);
        TEST_ASSERT_FALSE(loaded.display.stayOnAtNight);
        TEST_ASSERT_FALSE(loaded.display.rotate180);
        TEST_ASSERT_EQUAL_UINT8(255, loaded.display.buttonPin);
    }
}

// Spec 032 (FR-016): settings from before the battery (a release and a
// device set up on this branch with spec 031's display) load with every value
// kept and the battery at its defaults; stored alerts without the battery part
// (no "alertsBatt" key yet) get the six default battery events.
void test_pre_032_config_gets_battery_defaults()
{
    for (const char *tag : {"v0.3.3-beta.1", "pre-032-travel-allsky"})
    {
        const std::string stored = readFile(std::string("test/fixtures/config-releases/") + tag + ".json");
        TEST_ASSERT_FALSE_MESSAGE(stored.empty(), tag);
        TEST_ASSERT_TRUE(stored.find("\"battery\":") == std::string::npos);
        TEST_ASSERT_TRUE(stored.find("battery_low") == std::string::npos);
        Config loaded = Config::createDefault();
        std::string reason;
        TEST_ASSERT_TRUE_MESSAGE(Config::applyJson(stored, loaded, false, &reason), reason.c_str());
        const Config defaults = Config::createDefault();
        const std::string want = defaults.toJson(false, false);
        const std::string got = loaded.toJson(false, false);
        const size_t wantAt = want.find("\"battery\":{");
        const size_t gotAt = got.find("\"battery\":{");
        TEST_ASSERT_TRUE(wantAt != std::string::npos && gotAt != std::string::npos);
        TEST_ASSERT_EQUAL_STRING(
            want.substr(wantAt, want.find('}', wantAt) - wantAt).c_str(), got.substr(gotAt, got.find('}', gotAt) - gotAt).c_str());
        TEST_ASSERT_FALSE(loaded.alpaca.batteryEnabled);
        TEST_ASSERT_EQUAL_UINT8(10, loaded.alpaca.batteryMinPercent);
        TEST_ASSERT_TRUE(loaded.mqtt.publish.battery);
        TEST_ASSERT_EQUAL_UINT8(2, loaded.alerts.batteryLow.level);
        TEST_ASSERT_EQUAL_UINT8(4, loaded.alerts.batteryCritical.level);
        TEST_ASSERT_EQUAL_UINT8(4, loaded.alerts.batteryShutdown.level);
        TEST_ASSERT_EQUAL_UINT8(0, loaded.alerts.powerRestored.level);
        TEST_ASSERT_EQUAL_UINT8(0, loaded.alerts.chargeComplete.level);
        TEST_ASSERT_EQUAL_UINT8(2, loaded.alerts.chargeFault.level);
    }
    // The spec 031 device's display survives the upgrade.
    const std::string stored = readFile("test/fixtures/config-releases/pre-032-travel-allsky.json");
    Config loaded = Config::createDefault();
    TEST_ASSERT_TRUE(Config::applyJson(stored, loaded, false));
    TEST_ASSERT_TRUE(loaded.display.enabled);
    TEST_ASSERT_EQUAL_UINT8(33, loaded.display.buttonPin);

    // As ConfigStore splices it: main and alerts parts, no battery part.
    const Config c = Config::createDefault();
    const std::string spliced = Config::spliceStored(
        c.toJson(false, false), c.alertsToJson(false, Config::AlertsPart::Main), c.alertsToJson(false, Config::AlertsPart::Client), "");
    TEST_ASSERT_TRUE(spliced.find("alertsBattery") == std::string::npos);
    const auto fromStore = Config::fromJson(spliced);
    TEST_ASSERT_TRUE(fromStore.has_value());
    TEST_ASSERT_EQUAL_UINT8(4, fromStore->alerts.batteryCritical.level);
}

void test_pre_032_travel_allsky()
{
    loadRelease("pre-032-travel-allsky");
}

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
    for (const char *name :
         {"unsafe", "safe", "rain_started", "rain_stopped", "sensor_fault", "sensor_recovered", "dew_risk", "clear_sky", "clouded_over"})
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
    RUN_TEST(test_v0_3_3_beta_1);
    RUN_TEST(test_pre_030_config_gets_sky_defaults);
    RUN_TEST(test_pre_031_config_gets_display_defaults);
    RUN_TEST(test_pre_032_config_gets_battery_defaults);
    RUN_TEST(test_pre_032_travel_allsky);
    return UNITY_END();
}
