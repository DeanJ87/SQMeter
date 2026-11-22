#include <unity.h>

#include <ArduinoJson.h>

#include <cmath>

#include "DeviceCore.h"

// Rain needs cloud (specs/036-rain-needs-cloud): what the device feeds the
// rule, and the safety document's dew flag.

using namespace SQM;

namespace
{
    constexpr uint32_t NOW = 100000;

    // Fresh sky sensors, a clear sky (-25 C sky, 12 C air) and a raining RG-15.
    SensorSnapshot wetClearNight()
    {
        SensorSnapshot s;
        s.skySensors[0].initialized = s.bmeInitialized = s.mlxInitialized = s.rg15Initialized = true;
        s.skySensors[0].reading.status = s.bme.status = s.mlx.status = s.rg15.status = SensorStatus::Ok;
        s.skySensors[0].reading.timestamp = s.bme.timestamp = s.mlx.timestamp = NOW - 500;
        s.skySensors[0].lastUpdate = s.bmeLastUpdate = s.mlxLastUpdate = NOW - 500;
        s.dataTimestamp = NOW - 500;
        s.bme.temperature = 12.0F;
        s.bme.humidity = 60.0F;
        s.mlx.objectTemp = -25.0F;
        s.mlx.ambientTemp = 12.0F;
        s.rg15.online = true;
        s.rg15.timestamp = NOW - 100;
        s.rg15.isRaining = true;
        s.rg15.rInt = 0.12F;
        return s;
    }

    Config needsCloud()
    {
        Config c = Config::createDefault();
        c.rain.enabled = true;
        c.alpaca.rainNeedsCloud = true;
        return c;
    }

    Alpaca::SafetyInputs inputs(SensorSnapshot s, const Config &cfg)
    {
        Core::derive(s, cfg);
        return Core::safetyInputs(s, cfg, NOW);
    }
} // namespace

void test_rain_inputs_healthy_clear_sky_is_usable()
{
    const Alpaca::SafetyInputs in = inputs(wetClearNight(), needsCloud());
    TEST_ASSERT_TRUE(in.skyCloudUsable);
    TEST_ASSERT_TRUE(in.cloudCoverPercent < 10.0F);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.12F, in.rainRateMmPerHour);
}

void test_rain_inputs_ir_missing_faulted_stale_or_uncomputable_not_usable()
{
    const Config cfg = needsCloud();
    SensorSnapshot missing = wetClearNight();
    missing.mlxInitialized = false;
    TEST_ASSERT_FALSE(inputs(missing, cfg).skyCloudUsable);

    SensorSnapshot faulted = wetClearNight();
    faulted.mlx.status = SensorStatus::ReadError;
    TEST_ASSERT_FALSE(inputs(faulted, cfg).skyCloudUsable);

    SensorSnapshot stale = wetClearNight();
    stale.mlxLastUpdate = NOW - 600000;
    TEST_ASSERT_FALSE(inputs(stale, cfg).skyCloudUsable);

    SensorSnapshot uncomputable = wetClearNight();
    uncomputable.mlx.objectTemp = NAN;
    TEST_ASSERT_FALSE(inputs(uncomputable, cfg).skyCloudUsable);
}

void test_rain_device_pass_dew_then_document()
{
    const Config cfg = needsCloud();
    Alpaca::SafetyInputs in = inputs(wetClearNight(), cfg);
    const Alpaca::SafetyThresholds t = Core::safetyThresholds(cfg);
    Alpaca::RainTrustLatch latch;
    Alpaca::applyRainTrust(in, t, latch);
    const Alpaca::SafetyResult result = Alpaca::evaluateSafety(in, t);
    TEST_ASSERT_TRUE(result.dewSuspected);
    TEST_ASSERT_FALSE(result.reasonFlags & Alpaca::UnsafeRain);

    SafetyStatus status;
    Alpaca::SafeDelayFilter filter;
    Core::updateSafety(status, filter, result, cfg, NOW);
    TEST_ASSERT_TRUE(status.dewSuspected);
    TEST_ASSERT_EQUAL_UINT32(NOW, status.dewSinceMs);
    Core::updateSafety(status, filter, result, cfg, NOW + 1000);
    TEST_ASSERT_EQUAL_UINT32(NOW, status.dewSinceMs); // the spell's start is kept

    DynamicJsonDocument doc(1536);
    Core::writeSafety(doc.to<JsonObject>(), status, cfg, NOW + 5000);
    TEST_ASSERT_TRUE(doc["dewSuspected"].as<bool>());
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.12F, doc["dew"]["rainIntensity"].as<float>());
    TEST_ASSERT_EQUAL(0, doc["dew"]["cloudCoverPercent"].as<int>());
    TEST_ASSERT_EQUAL_UINT32(5000, doc["dew"]["sinceAgeMs"].as<uint32_t>());
}

void test_rain_document_without_dew_has_flag_only()
{
    SafetyStatus status;
    DynamicJsonDocument doc(1536);
    Core::writeSafety(doc.to<JsonObject>(), status, needsCloud(), NOW);
    TEST_ASSERT_FALSE(doc["dewSuspected"].as<bool>());
    TEST_ASSERT_FALSE(doc.containsKey("dew"));
}
