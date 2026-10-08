#include <unity.h>

#include <ArduinoJson.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "SettingsDeps.h"

using namespace SQM;

// Settings dependencies (specs/020-settings-dependencies). The fixture cases
// are shared with web/src/lib/__tests__/settingsDeps.test.ts, so the device
// and the web UI's preview give the same answers. Catalogue IDs covered here:
// D-01 D-02 D-03 D-04 D-05 D-06 D-07 D-08 D-09 D-10 D-11 D-12 D-13 D-14 D-15
// D-16 D-17 D-18 D-19 D-23 D-24 D-25 D-26 D-28 D-29 D-30 D-31 D-32 D-35 D-36.

namespace
{
    std::string readFile(const char *path)
    {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    Deps::Facts factsFrom(JsonObjectConst base, JsonObjectConst overrides)
    {
        auto flag = [&](const char *key) { return overrides.containsKey(key) ? overrides[key].as<bool>() : base[key].as<bool>(); };
        Deps::Facts f;
        f.wifiConnected = flag("wifiConnected");
        f.mqttConnected = flag("mqttConnected");
        f.clockSet = flag("clockSet");
        f.gpsRunning = flag("gpsRunning");
        f.gpsFix = flag("gpsFix");
        f.bluetoothBuild = flag("bluetoothBuild");
        f.bluetoothRunning = flag("bluetoothRunning");
        f.pairedPhones =
            overrides.containsKey("pairedPhones") ? overrides["pairedPhones"].as<uint8_t>() : base["pairedPhones"].as<uint8_t>();
        f.lightDetected = flag("lightDetected");
        f.infraredDetected = flag("infraredDetected");
        f.environmentDetected = flag("environmentDetected");
        return f;
    }

    Deps::Facts healthy()
    {
        Deps::Facts f;
        f.wifiConnected = f.mqttConnected = f.clockSet = true;
        f.gpsRunning = f.gpsFix = f.bluetoothBuild = f.bluetoothRunning = true;
        f.pairedPhones = 1;
        f.lightDetected = f.infraredDetected = f.environmentDetected = true;
        return f;
    }

    // Applies `json` on top of `cfg` like a save, then stores and reloads it
    // like a restart.
    Config saveAndRestart(const Config &cfg, const char *json)
    {
        std::string error;
        auto next = Config::fromJson(json, &cfg, &error);
        if (!next)
            TEST_FAIL_MESSAGE(error.c_str());
        auto reloaded = Config::fromJson(next->toJson(false));
        TEST_ASSERT_TRUE(reloaded.has_value());
        return *reloaded;
    }

    std::string stateOf(const Config &cfg, const Deps::Facts &f, const char *setting)
    {
        const std::vector<Deps::Entry> entries = Deps::evaluate(cfg, f);
        const Deps::Entry *e = Deps::find(entries, setting);
        if (e == nullptr)
            return "missing";
        return std::string(Deps::stateName(e->state)) + (e->reason ? std::string(":") + e->reason->code : "");
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_fixture_cases()
{
    const std::string text = readFile("test/fixtures/settings-deps/cases.json");
    TEST_ASSERT_FALSE_MESSAGE(text.empty(), "run from the project root: test/fixtures/settings-deps/cases.json");
    DynamicJsonDocument doc(65536);
    TEST_ASSERT_FALSE(deserializeJson(doc, text));

    JsonObjectConst base = doc["baseFacts"];
    size_t checked = 0;
    for (JsonObjectConst c : doc["cases"].as<JsonArrayConst>())
    {
        const char *name = c["name"] | "?";
        std::string overlay;
        serializeJson(c["config"], overlay);
        std::string error;
        auto cfg = Config::fromJson(overlay, nullptr, &error);
        if (!cfg)
            TEST_FAIL_MESSAGE((std::string(name) + ": " + error).c_str());
        const std::vector<Deps::Entry> entries = Deps::evaluate(*cfg, factsFrom(base, c["facts"]));

        for (JsonPairConst expected : c["expect"].as<JsonObjectConst>())
        {
            const std::string where = std::string(name) + " / " + expected.key().c_str();
            const Deps::Entry *e = Deps::find(entries, expected.key().c_str());
            if (expected.value().isNull())
            {
                TEST_ASSERT_NULL_MESSAGE(e, where.c_str());
                continue;
            }
            TEST_ASSERT_NOT_NULL_MESSAGE(e, where.c_str());
            JsonObjectConst x = expected.value();
            TEST_ASSERT_EQUAL_STRING_MESSAGE(x["state"].as<const char *>(), Deps::stateName(e->state), where.c_str());
            TEST_ASSERT_EQUAL_STRING_MESSAGE(x["id"].as<const char *>(), e->id, where.c_str());
            if (e->state == Deps::State::Inactive)
            {
                TEST_ASSERT_NOT_NULL_MESSAGE(e->reason, where.c_str());
                TEST_ASSERT_EQUAL_STRING_MESSAGE(x["reason"].as<const char *>(), e->reason->code, where.c_str());
                if (x.containsKey("text"))
                    TEST_ASSERT_EQUAL_STRING_MESSAGE(x["text"].as<const char *>(), e->reason->text, where.c_str());
                if (x.containsKey("fix"))
                    TEST_ASSERT_EQUAL_STRING_MESSAGE(x["fix"].as<const char *>(), e->reason->fix, where.c_str());
            }
            else
            {
                TEST_ASSERT_FALSE_MESSAGE(x.containsKey("reason"), where.c_str());
            }
            const char *unmet = Deps::unmetName(e->unmet);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(x["unmet"] | "", unmet ? unmet : "", where.c_str());
            TEST_ASSERT_EQUAL_MESSAGE(x["neutral"] | false, e->neutral, where.c_str());
            ++checked;
        }
    }
    TEST_ASSERT_GREATER_THAN(60, checked);
}

void test_reasons_and_settings_match_catalogue()
{
    DynamicJsonDocument doc(32768);
    TEST_ASSERT_FALSE(deserializeJson(doc, readFile("lib/SettingsDeps/catalogue.json")));

    JsonObjectConst reasons = doc["reasons"];
    TEST_ASSERT_EQUAL(reasons.size(), Deps::reasons().size());
    for (const Deps::Reason &r : Deps::reasons())
    {
        TEST_ASSERT_TRUE_MESSAGE(reasons.containsKey(r.code), r.code);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reasons[r.code]["text"].as<const char *>(), r.text, r.code);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reasons[r.code]["fix"].as<const char *>(), r.fix, r.code);
    }

    // Every reported setting is in a "setting" entry of the catalogue, and
    // every such setting is reported.
    std::set<std::string> catalogued;
    for (JsonObjectConst entry : doc["entries"].as<JsonArrayConst>())
        if (std::strcmp(entry["kind"] | "", "setting") == 0)
            for (JsonVariantConst s : entry["settings"].as<JsonArrayConst>())
                catalogued.insert(s.as<const char *>());
    std::set<std::string> reported;
    for (const Deps::Entry &e : Deps::evaluate(Config::createDefault(), healthy()))
    {
        TEST_ASSERT_TRUE_MESSAGE(catalogued.count(e.setting) == 1, e.setting);
        reported.insert(e.setting);
    }
    for (const std::string &s : catalogued)
        TEST_ASSERT_TRUE_MESSAGE(reported.count(s) == 1, s.c_str());
}

// The web parity test applies fixture configs on top of this file; it must
// be the device's defaults. UPDATE_FIXTURES=1 rewrites it.
void test_default_config_fixture_is_current()
{
    const char *path = "test/fixtures/settings-deps/default-config.json";
    const std::string current = Config::createDefault().toJson(false);
    if (std::getenv("UPDATE_FIXTURES") != nullptr)
    {
        std::ofstream(path) << current << "\n";
        return;
    }
    std::string stored = readFile(path);
    while (!stored.empty() && (stored.back() == '\n' || stored.back() == '\r'))
        stored.pop_back();
    TEST_ASSERT_EQUAL_STRING_MESSAGE(current.c_str(), stored.c_str(), "run UPDATE_FIXTURES=1 pio test -e native -f test_settings_deps");
}

// D-15: rain rules are ignored (not fail-safe) with the rain sensor off, and
// the safety document says so.
void test_rules_not_in_effect()
{
    Config cfg = Config::createDefault();
    cfg.alpaca.rainUnsafeEnabled = true;
    cfg.alpaca.rainSensorRequired = true;
    cfg.rain.enabled = false;
    std::vector<std::string> rules = Deps::rulesNotInEffect(cfg);
    TEST_ASSERT_EQUAL(2, rules.size());
    TEST_ASSERT_EQUAL_STRING("Unsafe while raining - rain sensor is off", rules[0].c_str());

    cfg.rain.enabled = true;
    TEST_ASSERT_EQUAL(0, Deps::rulesNotInEffect(cfg).size());
}

void test_report_document()
{
    Config cfg = Config::createDefault();
    cfg.alerts.enabled = true;
    cfg.alerts.mqttEnabled = true;
    Deps::Facts f = healthy();
    f.mqttConnected = false;
    const std::vector<Deps::Entry> entries = Deps::evaluate(cfg, f);
    DynamicJsonDocument doc(Deps::reportCapacity(entries));
    Deps::writeReport(doc.to<JsonObject>(), entries, f);
    TEST_ASSERT_FALSE(doc.overflowed());
    std::string json;
    serializeJson(doc, json);
    TEST_ASSERT_LESS_THAN(5000, json.size());

    TEST_ASSERT_FALSE(doc["facts"]["mqttConnected"].as<bool>());
    JsonObject mqtt;
    for (JsonObject item : doc["settings"].as<JsonArray>())
        if (std::strcmp(item["setting"] | "", "alerts.mqtt.enabled") == 0)
            mqtt = item;
    TEST_ASSERT_FALSE(mqtt.isNull());
    TEST_ASSERT_EQUAL_STRING("D-01", mqtt["id"]);
    TEST_ASSERT_EQUAL_STRING("inactive", mqtt["state"]);
    TEST_ASSERT_EQUAL_STRING("mqtt-off", mqtt["reason"]);
    TEST_ASSERT_EQUAL_STRING("MQTT is off", mqtt["text"]);
    TEST_ASSERT_EQUAL_STRING("network#mqtt", mqtt["fix"]);
}

// US2 / FR-008 / SC-003: switching a dependency off, saving, restarting,
// switching it on and saving leaves every dependent value unchanged and
// active again.
void test_dependents_survive_dependency_round_trip()
{
    const Deps::Facts f = healthy();
    Config cfg = Config::createDefault();
    cfg = saveAndRestart(cfg, R"({"alerts":{"enabled":true,"mqtt":{"enabled":true},"armWithAlpaca":true,
        "events":{"rain_started":{"level":4},"rain_stopped":{"level":2}}},
        "mqtt":{"enabled":true,"broker":"192.168.1.10","topic":"sqmeter","homeAssistant":{"enabled":true},"publish":{"rain":true,"wind":true}},
        "rain":{"enabled":true,"dailyResetEnabled":true},"wind":{"enabled":true,"directionEnabled":true},"alpaca":{"enabled":true,
        "rainUnsafeEnabled":true,"rainSensorRequired":true,"windSpeedUnsafeEnabled":true,"windGustUnsafeEnabled":true}})");
    const std::string before = cfg.toJson(false);
    const char *dependents[] = {
        "alerts.mqtt.enabled",
        "alerts.armWithAlpaca",
        "alerts.events.rain_started.level",
        "mqtt.homeAssistant.alertsSwitch",
        "mqtt.publish.rain",
        "mqtt.publish.wind",
        "rain.dailyResetEnabled",
        "wind.directionEnabled",
        "alpaca.rainUnsafeEnabled",
        "alpaca.rainSensorRequired",
        "alpaca.windSpeedUnsafeEnabled",
        "alpaca.windGustUnsafeEnabled"};
    for (const char *setting : dependents)
        TEST_ASSERT_EQUAL_STRING_MESSAGE("active", stateOf(cfg, f, setting).c_str(), setting);

    struct
    {
        const char *off;
        const char *on;
        const char *dependent;
        const char *reason;
    } trips[] = {
        {R"({"mqtt":{"enabled":false}})", R"({"mqtt":{"enabled":true}})", "alerts.mqtt.enabled", "inactive:mqtt-off"},
        {R"({"rain":{"enabled":false}})", R"({"rain":{"enabled":true}})", "alerts.events.rain_started.level", "inactive:rain-off"},
        {R"({"wind":{"enabled":false}})", R"({"wind":{"enabled":true}})", "alpaca.windSpeedUnsafeEnabled", "inactive:wind-off"},
        {R"({"alpaca":{"enabled":false}})", R"({"alpaca":{"enabled":true}})", "alerts.armWithAlpaca", "inactive:alpaca-off"},
        {R"({"alerts":{"enabled":false}})", R"({"alerts":{"enabled":true}})", "mqtt.homeAssistant.alertsSwitch", "inactive:alerts-off"},
    };
    for (const auto &trip : trips)
    {
        Config off = saveAndRestart(cfg, trip.off);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(trip.reason, stateOf(off, f, trip.dependent).c_str(), trip.off);
        Config on = saveAndRestart(off, trip.on);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(before.c_str(), on.toJson(false).c_str(), trip.on);
        for (const char *setting : dependents)
            TEST_ASSERT_EQUAL_STRING_MESSAGE("active", stateOf(on, f, setting).c_str(), setting);
    }
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_fixture_cases);
    RUN_TEST(test_reasons_and_settings_match_catalogue);
    RUN_TEST(test_default_config_fixture_is_current);
    RUN_TEST(test_rules_not_in_effect);
    RUN_TEST(test_report_document);
    RUN_TEST(test_dependents_survive_dependency_round_trip);
    return UNITY_END();
}
