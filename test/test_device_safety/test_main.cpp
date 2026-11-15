#include <unity.h>

#include <ArduinoJson.h>

#include "DeviceCore.h"

using namespace SQM;

// The safe and unsafe delays applied to the device's verdict (updateSafety),
// and what the safety document and alerts see.

namespace
{
    // A healthy reading set: all three sky sensors fresh at `now`.
    SensorSnapshot healthy(uint32_t now)
    {
        SensorSnapshot s;
        s.skySensors[0].initialized = s.bmeInitialized = s.mlxInitialized = true;
        s.skySensors[0].reading.status = s.bme.status = s.mlx.status = SensorStatus::Ok;
        s.skySensors[0].reading.timestamp = s.bme.timestamp = s.mlx.timestamp = now - 500;
        s.skySensors[0].lastUpdate = s.bmeLastUpdate = s.mlxLastUpdate = now - 500;
        s.dataTimestamp = now - 500;
        s.skySensors[0].reading.lux = 0.0003f;
        s.bme.temperature = 12.0f;
        s.bme.humidity = 60.0f;
        s.bme.pressure = 1013.0f;
        s.bme.dewpoint = 4.5f;
        s.mlx.objectTemp = -25.0f;
        s.mlx.ambientTemp = 12.0f;
        s.skySensors[0].diagnostics.integrationMs = 600;
        s.skySensors[0].diagnostics.averagingWindowSeconds = 90;
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

void test_safety_with_unsafe_delay()
{
    const uint32_t now = 100000;
    SensorSnapshot s = healthy(now);
    Config cfg = defaults();
    cfg.alpaca.unsafeDelaySeconds = 30;
    Core::derive(s, cfg);

    SafetyStatus status;
    Alpaca::SafeDelayFilter filter;
    const auto safeResult = Alpaca::evaluateSafety(Core::safetyInputs(s, cfg, now), Core::safetyThresholds(cfg));
    Core::updateSafety(status, filter, safeResult, cfg, now);
    TEST_ASSERT_TRUE(status.isSafe);

    // Cloud limit under the reading: still reported safe, counting down,
    // with the raw reasons.
    cfg.alpaca.cloudCoverUnsafePercent = 0.0f;
    const auto cloudy = Alpaca::evaluateSafety(Core::safetyInputs(s, cfg, now + 1000), Core::safetyThresholds(cfg));
    TEST_ASSERT_FALSE(Core::updateSafety(status, filter, cloudy, cfg, now + 1000));
    TEST_ASSERT_TRUE(status.isSafe);
    TEST_ASSERT_FALSE(status.rawSafe);
    TEST_ASSERT_EQUAL_UINT32(30, status.secondsUntilUnsafe);

    StaticJsonDocument<512> doc;
    Core::writeSafety(doc.to<JsonObject>(), status, cfg, now + 1000);
    TEST_ASSERT_TRUE(doc["safe"].as<bool>());
    TEST_ASSERT_FALSE(doc["rawSafe"].as<bool>());
    TEST_ASSERT_EQUAL(30, doc["secondsUntilUnsafe"].as<int>());
    TEST_ASSERT_TRUE(doc["reasons"].size() >= 1);

    // Alerts follow the reported verdict: no reasons while held back.
    const Core::AlertSources sources{cfg, Core::observingConditions(s, cfg, now), Core::night(s, cfg, 0), status};
    TEST_ASSERT_EQUAL_UINT32(0, Core::alertInputs(sources, s, now + 1000).unsafeFlags);

    TEST_ASSERT_TRUE(Core::updateSafety(status, filter, cloudy, cfg, now + 31000));
    TEST_ASSERT_FALSE(status.isSafe);
    TEST_ASSERT_EQUAL_UINT32(0, status.secondsUntilUnsafe);
    TEST_ASSERT_EQUAL_UINT32(Alpaca::UnsafeCloudCover, Core::alertInputs(sources, s, now + 31000).unsafeFlags);
}

void setUp() {}
void tearDown() {}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_safety_with_safe_delay);
    RUN_TEST(test_safety_with_unsafe_delay);
    return UNITY_END();
}
