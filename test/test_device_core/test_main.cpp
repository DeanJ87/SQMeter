#include <unity.h>

#include <ArduinoJson.h>

#include "DeviceCore.h"
#include "ObservingConditionsMapper.h"

#include <fstream>
#include <set>
#include <sstream>

using namespace SQM;

namespace
{
    constexpr int64_t NIGHT_EPOCH = 1791417600;  // 2026-10-08 00:00 UTC (dark over London)
    constexpr int64_t MIDDAY_EPOCH = 1791460800; // 2026-10-08 12:00 UTC

    // A healthy reading set: all three sky sensors fresh at `now`.
    SensorSnapshot healthy(uint32_t now)
    {
        SensorSnapshot s;
        s.tslInitialized = s.bmeInitialized = s.mlxInitialized = true;
        s.tsl.status = s.bme.status = s.mlx.status = SensorStatus::OK;
        s.tsl.timestamp = s.bme.timestamp = s.mlx.timestamp = now - 500;
        s.tslLastUpdate = s.bmeLastUpdate = s.mlxLastUpdate = now - 500;
        s.dataTimestamp = now - 500;
        s.tsl.lux = 0.0003f;
        s.bme.temperature = 12.0f;
        s.bme.humidity = 60.0f;
        s.bme.pressure = 1013.0f;
        s.bme.dewpoint = 4.5f;
        s.mlx.objectTemp = -25.0f;
        s.mlx.ambientTemp = 12.0f;
        s.tslDiagnostics.integrationMs = 600;
        s.tslDiagnostics.averagingWindowSeconds = 90;
        return s;
    }

    Config defaults()
    {
        Config c = Config::createDefault();
        c.location.set = true;
        c.location.latitude = 51.48;
        c.location.longitude = 0.0;
        return c;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_derive_and_readings()
{
    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    const Config cfg = defaults();
    Core::derive(s, cfg);
    TEST_ASSERT_TRUE(s.humidityMeasured);
    TEST_ASSERT_TRUE(s.sky.sqm > 21.0f);

    const Readings::Snapshot r = Core::buildReadings(s, cfg, now, NIGHT_EPOCH);
    TEST_ASSERT_EQUAL(static_cast<int>(Readings::Status::Ok), static_cast<int>(r.light.status));
    TEST_ASSERT_EQUAL(static_cast<int>(Readings::Status::Ok), static_cast<int>(r.infrared.status));
    TEST_ASSERT_TRUE(r.timeValid);
    TEST_ASSERT_EQUAL(NIGHT_EPOCH, r.timestamp);
    TEST_ASSERT_EQUAL_STRING("clear", r.clouds.condition);
    TEST_ASSERT_FALSE(r.gps.present); // GPS off by default
    TEST_ASSERT_FALSE(r.rain.present);
}

void test_missing_stale_and_clock()
{
    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    s.mlxInitialized = false;
    s.mlx.status = SensorStatus::NOT_INITIALIZED;
    s.bmeLastUpdate = now - 20000; // older than read interval + grace
    const Config cfg = defaults();
    Core::derive(s, cfg);
    const Readings::Snapshot r = Core::buildReadings(s, cfg, now, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(Readings::Status::Missing), static_cast<int>(r.infrared.status));
    TEST_ASSERT_EQUAL(static_cast<int>(Readings::Status::Stale), static_cast<int>(r.environment.status));
    TEST_ASSERT_FALSE(r.timeValid);
    TEST_ASSERT_EQUAL(0, r.timestamp);

    StaticJsonDocument<512> doc;
    Core::writeSensorHealth(doc.to<JsonObject>(), r, cfg);
    TEST_ASSERT_EQUAL_STRING("missing", doc["infrared"]["status"]);
    TEST_ASSERT_FALSE(doc["infrared"].containsKey("ageMs"));
    TEST_ASSERT_TRUE(doc["environment"].containsKey("ageMs"));
}

void test_safety_with_safe_delay()
{
    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    Config cfg = defaults();
    cfg.alpaca.safeDelaySeconds = 60;
    Core::derive(s, cfg);

    SafetyStatus status;
    Alpaca::SafeDelayFilter filter;
    const auto safeResult = Alpaca::evaluateSafety(Core::safetyInputs(s, cfg, now), Core::safetyThresholds(cfg));
    TEST_ASSERT_TRUE(safeResult.isSafe);
    // First evaluation: reported unsafe while the safe delay runs.
    TEST_ASSERT_TRUE(Core::updateSafety(status, filter, safeResult, cfg, now));
    TEST_ASSERT_FALSE(status.isSafe);
    TEST_ASSERT_TRUE(status.rawSafe);
    TEST_ASSERT_TRUE(status.secondsUntilSafe > 0);
    // After the delay it's safe.
    TEST_ASSERT_TRUE(Core::updateSafety(status, filter, safeResult, cfg, now + 61000));
    TEST_ASSERT_TRUE(status.isSafe);

    // Tighten cloud cover below the reading: unsafe at once, with a reason.
    s.mlx.objectTemp = 8.0f; // overcast
    Core::derive(s, cfg);
    const auto cloudy = Alpaca::evaluateSafety(Core::safetyInputs(s, cfg, now + 62000), Core::safetyThresholds(cfg));
    TEST_ASSERT_FALSE(cloudy.isSafe);
    TEST_ASSERT_TRUE(Core::updateSafety(status, filter, cloudy, cfg, now + 62000));
    TEST_ASSERT_FALSE(status.isSafe);

    StaticJsonDocument<512> doc;
    Core::writeSafety(doc.to<JsonObject>(), status, cfg, now + 63000);
    TEST_ASSERT_FALSE(doc["safe"].as<bool>());
    TEST_ASSERT_TRUE(doc["reasons"].size() >= 1);
    TEST_ASSERT_EQUAL(1000, doc["evaluatedAgeMs"].as<int>());
}

// D-15 (specs/020-settings-dependencies): rain rules switched on without the
// rain sensor are ignored, and the safety document says so.
void test_safety_lists_rules_not_in_effect()
{
    Config cfg = defaults();
    cfg.rain.enabled = false;
    cfg.alpaca.rainUnsafeEnabled = true;
    cfg.alpaca.rainSensorRequired = false;
    SafetyStatus status;
    DynamicJsonDocument doc(1536);
    Core::writeSafety(doc.to<JsonObject>(), status, cfg, 1000);
    TEST_ASSERT_EQUAL(1, doc["rulesNotInEffect"].size());
    TEST_ASSERT_EQUAL_STRING("Unsafe while raining - rain sensor is off", doc["rulesNotInEffect"][0]);

    cfg.rain.enabled = true;
    Core::writeSafety(doc.to<JsonObject>(), status, cfg, 1000);
    TEST_ASSERT_EQUAL(0, doc["rulesNotInEffect"].size());
}

void test_rain_makes_unsafe_even_when_stale()
{
    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    Config cfg = defaults();
    cfg.rain.enabled = true;
    s.rg15Initialized = true;
    s.rg15.online = true;
    s.rg15.stale = false;
    s.rg15.status = SensorStatus::OK;
    s.rg15.timestamp = now - 100;
    s.rg15.isRaining = true;
    s.rg15.rInt = 2.0f;
    s.dataTimestamp = now - 600000; // everything else stale
    s.tslLastUpdate = s.mlxLastUpdate = now - 600000;
    Core::derive(s, cfg);
    const auto result = Alpaca::evaluateSafety(Core::safetyInputs(s, cfg, now), Core::safetyThresholds(cfg));
    TEST_ASSERT_FALSE(result.isSafe);
    TEST_ASSERT_TRUE(result.reasonFlags & Alpaca::UNSAFE_RAIN);
    TEST_ASSERT_TRUE(result.reasonFlags & Alpaca::UNSAFE_STALE_DATA);
}

namespace
{
    // A rain sensor that is answering and dry.
    void dryRain(SensorSnapshot &s, Config &cfg, uint32_t now)
    {
        cfg.rain.enabled = true;
        s.rg15Initialized = true;
        s.rg15.online = true;
        s.rg15.stale = false;
        s.rg15.status = SensorStatus::OK;
        s.rg15.timestamp = now - 100;
    }

    Alpaca::SafetyResult verdict(SensorSnapshot &s, const Config &cfg, uint32_t now)
    {
        Core::derive(s, cfg);
        return Alpaca::evaluateSafety(Core::safetyInputs(s, cfg, now), Core::safetyThresholds(cfg));
    }
} // namespace

// specs/006 US1/AC4: "stale" means the sensors stopped giving fresh reads,
// not that the read loop stopped - a sensor still reporting OK with an old
// reading makes the verdict unsafe.
void test_stale_when_a_required_sensor_stops_refreshing()
{
    const uint32_t now = 1000000;
    SensorSnapshot s = healthy(now);
    const Config cfg = defaults();
    s.dataTimestamp = now - 100;    // the loop is running
    s.tslLastUpdate = now - 600000; // but the TSL2591 hasn't read for 10 min
    const auto result = verdict(s, cfg, now);
    TEST_ASSERT_FALSE(result.isSafe);
    TEST_ASSERT_TRUE(result.reasonFlags & Alpaca::UNSAFE_STALE_DATA);

    SensorSnapshot fresh = healthy(now);
    TEST_ASSERT_TRUE(verdict(fresh, cfg, now).isSafe);
}

// A sensor that never answered is a fault, not "no data" or "stale".
void test_missing_sensor_is_a_fault_not_stale()
{
    const uint32_t now = 1000000;
    SensorSnapshot s = healthy(now);
    const Config cfg = defaults();
    s.mlxInitialized = false;
    s.mlx.status = SensorStatus::NOT_INITIALIZED;
    s.mlxLastUpdate = 0;
    const auto result = verdict(s, cfg, now);
    TEST_ASSERT_FALSE(result.isSafe);
    TEST_ASSERT_TRUE(result.reasonFlags & Alpaca::UNSAFE_SENSOR_FAULT);
    TEST_ASSERT_FALSE(result.reasonFlags & Alpaca::UNSAFE_STALE_DATA);
    TEST_ASSERT_FALSE(result.reasonFlags & Alpaca::UNSAFE_NO_DATA);

    s.tslLastUpdate = 0;
    s.tsl.status = SensorStatus::READ_ERROR;
    TEST_ASSERT_TRUE(verdict(s, cfg, now).reasonFlags & Alpaca::UNSAFE_NO_DATA);
}

// specs/006: the rain sensor's health feeds the verdict - a dirty lens or a
// sensor that stopped answering is unsafe when the rain sensor is required.
void test_rain_sensor_lens_fault_and_stale_are_unsafe()
{
    const uint32_t now = 1000000;
    Config cfg = defaults();
    cfg.alpaca.rainUnsafeEnabled = true;
    cfg.alpaca.rainSensorRequired = true;

    SensorSnapshot s = healthy(now);
    dryRain(s, cfg, now);
    TEST_ASSERT_TRUE(verdict(s, cfg, now).isSafe);

    s.rg15.lensBad = true;
    auto result = verdict(s, cfg, now);
    TEST_ASSERT_FALSE(result.isSafe);
    TEST_ASSERT_TRUE(result.reasonFlags & Alpaca::UNSAFE_RAIN_SENSOR_FAULT);

    s.rg15.lensBad = false;
    s.rg15.stale = true;
    result = verdict(s, cfg, now);
    TEST_ASSERT_FALSE(result.isSafe);
    TEST_ASSERT_TRUE(result.reasonFlags & Alpaca::UNSAFE_RAIN_SENSOR_FAULT);

    s.rg15.stale = false;
    s.rg15.online = false;
    TEST_ASSERT_TRUE(verdict(s, cfg, now).reasonFlags & Alpaca::UNSAFE_RAIN_SENSOR_FAULT);
}

void test_night_from_location_and_clock()
{
    SensorSnapshot s;
    Config cfg = defaults();
    const Core::NightState night = Core::night(s, cfg, NIGHT_EPOCH);
    TEST_ASSERT_TRUE(night.known);
    TEST_ASSERT_TRUE(night.isNight);
    TEST_ASSERT_EQUAL_STRING("manual", night.source);
    TEST_ASSERT_FALSE(Core::night(s, cfg, MIDDAY_EPOCH).isNight);
    TEST_ASSERT_FALSE(Core::night(s, cfg, 0).known); // no clock

    s.gps.hasFix = true; // a GPS fix wins over settings
    s.gps.latitude = -33.9;
    s.gps.longitude = 151.2;
    TEST_ASSERT_EQUAL_STRING("gps", Core::night(s, cfg, NIGHT_EPOCH).source);

    cfg.location.set = false;
    s.gps.hasFix = false;
    const Core::NightState none = Core::night(s, cfg, NIGHT_EPOCH);
    StaticJsonDocument<256> doc;
    Core::writeSky(doc.to<JsonObject>(), none);
    TEST_ASSERT_EQUAL_STRING("none", doc["locationSource"]);
    TEST_ASSERT_FALSE(doc["nightKnown"].as<bool>());
    TEST_ASSERT_FALSE(doc.containsKey("sunAltitudeDeg"));
}

void test_alert_wording_and_levels()
{
    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    Config cfg = defaults();
    cfg.alerts.safetyNightOnly = false;
    cfg.alerts.unsafe.title = "{device}: {event} at {time}";
    cfg.alerts.unsafe.message = "SQM {sqm}, cloud {cloud}%";
    Core::derive(s, cfg);
    const auto obs = Core::observingConditions(s, cfg, now);
    const auto night = Core::night(s, cfg, NIGHT_EPOCH);

    Alerts::AlertEngine engine;
    SafetyStatus status;
    status.evaluatedAtMs = now;
    status.isSafe = true;
    status.rawSafe = true;
    // Baseline pass, then unsafe after the engine's 60 s start-up grace.
    Core::runAlerts(
        engine, Core::alertInputs(status, s, obs, cfg, night, now), Core::alertRules(cfg), cfg, obs, night, status, "23:00", "2026-10-07");
    status.isSafe = false;
    status.rawSafe = false;
    status.reasons = {"Cloud 96% >= 90%"};
    const Core::AlertStep step = Core::runAlerts(
        engine,
        Core::alertInputs(status, s, obs, cfg, night, now + 70000),
        Core::alertRules(cfg),
        cfg,
        obs,
        night,
        status,
        "23:01",
        "2026-10-07");
    TEST_ASSERT_EQUAL(1, step.outgoing.size());
    const Alerts::Alert &alert = step.outgoing[0];
    TEST_ASSERT_EQUAL(static_cast<int>(Alerts::AlertLevel::Urgent), static_cast<int>(alert.level));
    TEST_ASSERT_EQUAL_STRING("SQM-ESP32: unsafe at 23:01", alert.title.c_str());
    TEST_ASSERT_EQUAL_STRING_MESSAGE("SQM 21.41, cloud 0%", alert.message.c_str(), alert.message.c_str());
    TEST_ASSERT_TRUE(alert.vars.empty()); // cleared once applied

    // An event switched Off never goes out.
    cfg.alerts.safe.level = 0;
    status.isSafe = status.rawSafe = true;
    status.reasons.clear();
    const Core::AlertStep quiet = Core::runAlerts(
        engine,
        Core::alertInputs(status, s, obs, cfg, night, now + 600000),
        Core::alertRules(cfg),
        cfg,
        obs,
        night,
        status,
        "23:11",
        "2026-10-07");
    TEST_ASSERT_EQUAL(0, quiet.outgoing.size());
}

// specs/008 FR-006: the device fills exactly the common variables the web UI
// offers (lib/AlertLogic/template-variables.json).
void test_alert_vars_match_the_shared_list()
{
    std::ifstream in("lib/AlertLogic/template-variables.json");
    std::stringstream text;
    text << in.rdbuf();
    DynamicJsonDocument doc(4096);
    TEST_ASSERT_FALSE(deserializeJson(doc, text.str()));
    std::set<std::string> shared;
    for (JsonVariantConst name : doc["common"].as<JsonArrayConst>())
        shared.insert(name.as<std::string>());

    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    const Config cfg = defaults();
    Core::derive(s, cfg);
    Alerts::Alert alert; // no event values of its own
    std::set<std::string> filled;
    for (const auto &v :
         Core::alertVars(cfg, Core::observingConditions(s, cfg, now), Core::night(s, cfg, NIGHT_EPOCH), alert, "23:00", "2026-10-07"))
        filled.insert(v.first);
    TEST_ASSERT_TRUE_MESSAGE(filled == shared, "Core::alertVars and template-variables.json \"common\" differ");
}

void test_iso_utc_and_window()
{
    TEST_ASSERT_EQUAL_STRING("2026-10-08T00:00:00Z", Core::isoUtc(NIGHT_EPOCH).c_str());
    TEST_ASSERT_EQUAL_STRING("", Core::isoUtc(1000).c_str());
    TSL2591Diagnostics diag;
    diag.integrationMs = 600;
    diag.averagingWindowSeconds = 90;
    TEST_ASSERT_EQUAL(150, Core::windowSamples(diag));
    diag.averagingWindowSeconds = 600;
    TEST_ASSERT_EQUAL(512, Core::windowSamples(diag)); // buffer cap
}

// Spec 003 edge case: rain switched off in settings -> Alpaca RainRate is
// NotImplemented end to end, even with a stale reading still in the snapshot.
void test_rainrate_not_implemented_when_rain_disabled()
{
    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    s.rg15.online = true;
    s.rg15.rInt = 3.0f;
    s.rg15.timestamp = now - 500;
    Config cfg = defaults();
    cfg.rain.enabled = false;
    Core::derive(s, cfg);
    const auto off = Core::observingConditions(s, cfg, now);
    TEST_ASSERT_EQUAL(Alpaca::ALPACA_ERR_NOT_IMPLEMENTED, Alpaca::getObservingConditionsProperty("rainrate", off).errorNumber);

    cfg.rain.enabled = true;
    const auto on = Core::observingConditions(s, cfg, now);
    TEST_ASSERT_NOT_EQUAL(Alpaca::ALPACA_ERR_NOT_IMPLEMENTED, Alpaca::getObservingConditionsProperty("rainrate", on).errorNumber);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_derive_and_readings);
    RUN_TEST(test_missing_stale_and_clock);
    RUN_TEST(test_safety_with_safe_delay);
    RUN_TEST(test_safety_lists_rules_not_in_effect);
    RUN_TEST(test_rain_makes_unsafe_even_when_stale);
    RUN_TEST(test_stale_when_a_required_sensor_stops_refreshing);
    RUN_TEST(test_missing_sensor_is_a_fault_not_stale);
    RUN_TEST(test_rain_sensor_lens_fault_and_stale_are_unsafe);
    RUN_TEST(test_night_from_location_and_clock);
    RUN_TEST(test_alert_wording_and_levels);
    RUN_TEST(test_alert_vars_match_the_shared_list);
    RUN_TEST(test_iso_utc_and_window);
    RUN_TEST(test_rainrate_not_implemented_when_rain_disabled);
    return UNITY_END();
}
