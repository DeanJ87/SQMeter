#include "SafetyEvaluator.h"
#include <unity.h>

// The SafetyMonitor verdict (lib/AlpacaLogic SafetyEvaluator): thresholds,
// rain first, freshness, sensor faults, wind and the safe delay.

using namespace SQM::Alpaca;

void setUp(void) {}
void tearDown(void) {}

// --- SafetyEvaluator ---

void test_safe_when_all_thresholds_pass(void)
{
    SafetyThresholds t;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.secondsSinceLastGoodData = 5;
    in.cloudCoverPercent = 10.0f;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_TRUE(r.isSafe);
    TEST_ASSERT_EQUAL(0, r.unsafeReasons.size());
}

void test_unsafe_before_any_good_data(void)
{
    SafetyThresholds t;
    SafetyInputs in; // hasEverHadGoodData defaults false

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL(1, r.unsafeReasons.size());
}

void test_unsafe_when_data_stale(void)
{
    SafetyThresholds t;
    t.staleAfterSeconds = 30;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.secondsSinceLastGoodData = 31;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
}

void test_manual_override_forces_unsafe(void)
{
    SafetyThresholds t;
    t.manualOverrideUnsafe = true;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.secondsSinceLastGoodData = 0;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
}

void test_required_sensor_fault_forces_unsafe(void)
{
    SafetyThresholds t;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.requiredSensorFault = true;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
}

void test_cloud_cover_threshold(void)
{
    SafetyThresholds t;
    t.cloudCoverEnabled = true;
    t.cloudCoverUnsafePercent = 90.0f;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.cloudCoverPercent = 90.0f; // >= threshold => unsafe

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
}

void test_reasons_state_value_and_limit(void)
{
    SafetyThresholds t;
    t.cloudCoverEnabled = true;
    t.cloudCoverUnsafePercent = 35.0f;
    t.sqmMinEnabled = true;
    t.sqmMinSafe = 19.5f;
    t.humidityMaxEnabled = true;
    t.humidityMaxSafe = 90.0f;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.cloudCoverPercent = 62.0f;
    in.sqm = 18.21f;
    in.humidityPercent = 92.0f;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_EQUAL(3, r.unsafeReasons.size());
    TEST_ASSERT_EQUAL_STRING("Cloud 62% >= 35%", r.unsafeReasons[0].c_str());
    TEST_ASSERT_EQUAL_STRING("SQM 18.21 < 19.50", r.unsafeReasons[1].c_str());
    TEST_ASSERT_EQUAL_STRING("Humidity 92% > 90%", r.unsafeReasons[2].c_str());
}

void test_cloud_cover_disabled_ignored(void)
{
    SafetyThresholds t;
    t.cloudCoverEnabled = false;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.cloudCoverPercent = 100.0f;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_TRUE(r.isSafe);
}

void test_sqm_min_threshold(void)
{
    SafetyThresholds t;
    t.cloudCoverEnabled = false;
    t.sqmMinEnabled = true;
    t.sqmMinSafe = 18.0f;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.sqm = 17.9f;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
}

void test_humidity_max_threshold(void)
{
    SafetyThresholds t;
    t.cloudCoverEnabled = false;
    t.humidityMaxEnabled = true;
    t.humidityMaxSafe = 85.0f;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.humidityPercent = 90.0f;

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
}

void test_dewpoint_margin_threshold(void)
{
    SafetyThresholds t;
    t.cloudCoverEnabled = false;
    t.dewpointMarginEnabled = true;
    t.dewpointMarginMinC = 3.0f;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.temperatureC = 10.0f;
    in.dewpointC = 8.5f; // margin 1.5 < 3.0 => unsafe

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
}

void test_stale_data_suppresses_threshold_checks(void)
{
    // Stale data already makes it unsafe for a different reason; threshold
    // checks against stale/garbage readings shouldn't add misleading extras.
    SafetyThresholds t;
    t.staleAfterSeconds = 10;
    t.cloudCoverEnabled = true;
    t.cloudCoverUnsafePercent = 90.0f;
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.secondsSinceLastGoodData = 100;
    in.cloudCoverPercent = 5.0f; // would itself be safe

    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL(1, r.unsafeReasons.size()); // only the staleness reason
}

// --- ObservingConditionsMapper ---

namespace
{
    SafetyInputs freshSafeInputs()
    {
        SafetyInputs in;
        in.hasEverHadGoodData = true;
        in.secondsSinceLastGoodData = 1;
        in.cloudCoverPercent = 5.0f;
        in.rainSensorEnabled = true;
        in.rainSensorHealthy = true;
        return in;
    }
} // namespace

void test_rain_makes_unsafe(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);

    in.raining = true;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UnsafeRain, r.reasonFlags);
}

void test_rain_checked_even_when_other_data_stale(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    in.secondsSinceLastGoodData = 9999;
    in.raining = true;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_TRUE(r.reasonFlags & UnsafeRain);
    TEST_ASSERT_TRUE(r.reasonFlags & UnsafeStaleData);
}

// D-15: rain rules are not in effect (ignored) without the rain sensor.
void test_rain_rule_disabled_or_sensor_absent(void)
{
    SafetyThresholds t;
    t.rainUnsafeEnabled = false;
    SafetyInputs in = freshSafeInputs();
    in.raining = true;
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);

    t.rainUnsafeEnabled = true;
    in.rainSensorEnabled = false; // no RG-15 fitted
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);
}

void test_rain_sensor_required(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    in.rainSensorHealthy = false;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UnsafeRainSensorFault, r.reasonFlags);

    t.rainSensorRequired = false;
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);
}

void test_environment_fault_blocks_humidity_rules(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    in.environmentSensorFault = true;
    in.humidityPercent = 53.0f;                     // fallback value must not be trusted
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe); // rules disabled -> irrelevant

    t.humidityMaxEnabled = true;
    t.humidityMaxSafe = 90.0f;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UnsafeEnvironmentFault, r.reasonFlags);
}

void test_faulted_sensor_skips_its_threshold(void)
{
    SafetyThresholds t;
    t.sqmMinEnabled = true;
    t.sqmMinSafe = 18.0f;
    SafetyInputs in = freshSafeInputs();
    in.requiredSensorFault = true;
    in.irSkyFault = true;
    in.skyLightFault = true;
    in.cloudCoverPercent = 100.0f; // computed from zeroed MLX readings
    in.sqm = 0.0f;                 // from zeroed TSL readings
    TEST_ASSERT_EQUAL_UINT32(UnsafeSensorFault, evaluateSafety(in, t).reasonFlags);
}

void test_safe_delay_filter(void)
{
    SafeDelayFilter f;
    TEST_ASSERT_FALSE(f.update(true, 100000, 60)); // timer starts at first safe
    TEST_ASSERT_EQUAL_UINT32(60, f.secondsUntilSafe());
    TEST_ASSERT_FALSE(f.update(true, 159000, 60));
    TEST_ASSERT_TRUE(f.update(true, 160000, 60));
    TEST_ASSERT_FALSE(f.update(false, 161000, 60)); // unsafe is immediate
    TEST_ASSERT_FALSE(f.update(true, 162000, 60));  // and restarts the delay
    TEST_ASSERT_TRUE(f.update(true, 222000, 60));

    SafeDelayFilter immediate;
    TEST_ASSERT_TRUE(immediate.update(true, 5000, 0));
}

// millis() wraps after ~49.7 days; a wrap inside the delay must not report
// safe early (it did when the delay was measured in millis()/1000 seconds).
void test_safe_delay_filter_survives_millis_wrap(void)
{
    SafeDelayFilter f;
    const uint32_t beforeWrap = 0xFFFFFFFFu - 10000; // 10 s before the wrap
    TEST_ASSERT_FALSE(f.update(true, beforeWrap, 60));
    TEST_ASSERT_FALSE(f.update(true, beforeWrap + 20000, 60)); // wrapped, 20 s in
    TEST_ASSERT_EQUAL_UINT32(40, f.secondsUntilSafe());
    TEST_ASSERT_FALSE(f.update(true, beforeWrap + 59000, 60));
    TEST_ASSERT_TRUE(f.update(true, beforeWrap + 60000, 60));
}

// --- Unsafe delay ---

namespace
{
    // Reported safe at nowMs, with no safe delay.
    SafeDelayFilter reportedSafe(uint32_t nowMs)
    {
        SafeDelayFilter f;
        TEST_ASSERT_TRUE(f.update(true, nowMs, 0, 30));
        return f;
    }
} // namespace

void test_unsafe_delay_off_is_immediate(void)
{
    SafeDelayFilter f = reportedSafe(1000);
    TEST_ASSERT_FALSE(f.update(false, 2000, 0, 0, UnsafeCloudCover));
    TEST_ASSERT_EQUAL_UINT32(0, f.secondsUntilUnsafe());
}

void test_unsafe_delay_ignores_a_brief_unsafe(void)
{
    SafeDelayFilter f = reportedSafe(1000);
    TEST_ASSERT_TRUE(f.update(false, 2000, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_EQUAL_UINT32(30, f.secondsUntilUnsafe());
    TEST_ASSERT_TRUE(f.update(false, 12000, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_EQUAL_UINT32(20, f.secondsUntilUnsafe());
    TEST_ASSERT_TRUE(f.update(true, 13000, 0, 30)); // cleared: never reported unsafe
    TEST_ASSERT_EQUAL_UINT32(0, f.secondsUntilUnsafe());
}

void test_unsafe_delay_reports_after_the_delay(void)
{
    SafeDelayFilter f = reportedSafe(1000);
    TEST_ASSERT_TRUE(f.update(false, 2000, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_TRUE(f.update(false, 31000, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_EQUAL_UINT32(1, f.secondsUntilUnsafe());
    TEST_ASSERT_FALSE(f.update(false, 32000, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_EQUAL_UINT32(0, f.secondsUntilUnsafe());
    TEST_ASSERT_FALSE(f.update(false, 40000, 0, 30, UnsafeCloudCover)); // stays unsafe
}

void test_unsafe_delay_resets_when_safe_again(void)
{
    SafeDelayFilter f = reportedSafe(1000);
    TEST_ASSERT_TRUE(f.update(false, 2000, 0, 30, UnsafeHumidity));
    TEST_ASSERT_TRUE(f.update(false, 25000, 0, 30, UnsafeHumidity));
    TEST_ASSERT_TRUE(f.update(true, 26000, 0, 30));
    TEST_ASSERT_TRUE(f.update(false, 27000, 0, 30, UnsafeHumidity)); // starts again
    TEST_ASSERT_EQUAL_UINT32(30, f.secondsUntilUnsafe());
    TEST_ASSERT_TRUE(f.update(false, 56000, 0, 30, UnsafeHumidity));
    TEST_ASSERT_FALSE(f.update(false, 57000, 0, 30, UnsafeHumidity));
}

void test_unsafe_delay_bypassed_by_rain_override_and_shutdown(void)
{
    const uint32_t immediate[] = {UnsafeRain, UnsafeManualOverride, UnsafeShutdown, UnsafeCloudCover | UnsafeRain};
    for (uint32_t flags : immediate)
    {
        SafeDelayFilter f = reportedSafe(1000);
        TEST_ASSERT_FALSE(f.update(false, 2000, 0, 30, flags));
        TEST_ASSERT_EQUAL_UINT32(0, f.secondsUntilUnsafe());
    }
    // Rain arriving while a cloud reading is being held back.
    SafeDelayFilter f = reportedSafe(1000);
    TEST_ASSERT_TRUE(f.update(false, 2000, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_FALSE(f.update(false, 3000, 0, 30, UnsafeCloudCover | UnsafeRain));
}

void test_unsafe_delay_then_safe_delay(void)
{
    SafeDelayFilter f;
    TEST_ASSERT_FALSE(f.update(true, 0, 60, 30));
    TEST_ASSERT_TRUE(f.update(true, 60000, 60, 30));
    // A brief unsafe inside the unsafe delay doesn't restart the safe delay.
    TEST_ASSERT_TRUE(f.update(false, 61000, 60, 30, UnsafeCloudCover));
    TEST_ASSERT_TRUE(f.update(true, 62000, 60, 30));
    TEST_ASSERT_EQUAL_UINT32(0, f.secondsUntilSafe());
    // Once reported unsafe, the safe delay applies as before.
    TEST_ASSERT_TRUE(f.update(false, 63000, 60, 30, UnsafeCloudCover));
    TEST_ASSERT_FALSE(f.update(false, 93000, 60, 30, UnsafeCloudCover));
    TEST_ASSERT_FALSE(f.update(true, 94000, 60, 30));
    TEST_ASSERT_EQUAL_UINT32(60, f.secondsUntilSafe());
    TEST_ASSERT_TRUE(f.update(true, 154000, 60, 30));
}

// The unsafe delay only holds a reported safe: a booting device is unsafe
// until the safe delay has run, whatever the unsafe delay.
void test_unsafe_delay_never_reports_safe_at_boot(void)
{
    SafeDelayFilter f;
    TEST_ASSERT_FALSE(f.update(false, 1000, 60, 30, UnsafeNoData));
    TEST_ASSERT_EQUAL_UINT32(0, f.secondsUntilUnsafe());
    TEST_ASSERT_FALSE(f.update(true, 2000, 60, 30));
    TEST_ASSERT_FALSE(f.update(false, 3000, 60, 30, UnsafeCloudCover)); // still unsafe, delay restarts
    TEST_ASSERT_FALSE(f.update(true, 4000, 60, 30));
    TEST_ASSERT_EQUAL_UINT32(60, f.secondsUntilSafe());
}

void test_unsafe_delay_survives_millis_wrap(void)
{
    const uint32_t beforeWrap = 0xFFFFFFFFu - 10000; // 10 s before the wrap
    SafeDelayFilter f = reportedSafe(beforeWrap - 1000);
    TEST_ASSERT_TRUE(f.update(false, beforeWrap, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_TRUE(f.update(false, beforeWrap + 20000, 0, 30, UnsafeCloudCover)); // wrapped
    TEST_ASSERT_EQUAL_UINT32(10, f.secondsUntilUnsafe());
    TEST_ASSERT_TRUE(f.update(false, beforeWrap + 29000, 0, 30, UnsafeCloudCover));
    TEST_ASSERT_FALSE(f.update(false, beforeWrap + 30000, 0, 30, UnsafeCloudCover));
}

// --- Wind safety ---

void test_wind_limits(void)
{
    SafetyThresholds t;
    t.windSpeedUnsafeEnabled = true;
    t.windSpeedUnsafeMs = 10.0f;
    t.windGustUnsafeEnabled = true;
    t.windGustUnsafeMs = 15.0f;
    SafetyInputs in = freshSafeInputs();
    in.windSensorEnabled = true;
    in.windSensorHealthy = true;
    in.windSpeedMs = 5.0f;
    in.windGustMs = 9.0f;
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);

    in.windSpeedMs = 10.0f;
    TEST_ASSERT_EQUAL_UINT32(UnsafeWind, evaluateSafety(in, t).reasonFlags);

    in.windSpeedMs = 5.0f;
    in.windGustMs = 16.0f;
    TEST_ASSERT_EQUAL_UINT32(UnsafeWindGust, evaluateSafety(in, t).reasonFlags);
}

// D-16: a wind limit without an anemometer is fail-safe (unsafe).
void test_wind_limit_without_sensor_is_unsafe(void)
{
    SafetyThresholds t;
    t.windGustUnsafeEnabled = true;
    SafetyInputs in = freshSafeInputs();
    TEST_ASSERT_EQUAL_UINT32(UnsafeWindSensorFault, evaluateSafety(in, t).reasonFlags);

    t.windGustUnsafeEnabled = false; // no wind rules -> anemometer irrelevant
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);
}

// --- AlpacaRouter ---

void test_sky_fault_reasons_per_role();
void test_sky_limit_reasons_per_role();
void run_battery_rule_tests(void);

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_safe_when_all_thresholds_pass);
    RUN_TEST(test_unsafe_before_any_good_data);
    RUN_TEST(test_unsafe_when_data_stale);
    RUN_TEST(test_manual_override_forces_unsafe);
    RUN_TEST(test_required_sensor_fault_forces_unsafe);
    RUN_TEST(test_cloud_cover_threshold);
    RUN_TEST(test_reasons_state_value_and_limit);
    RUN_TEST(test_cloud_cover_disabled_ignored);
    RUN_TEST(test_sqm_min_threshold);
    RUN_TEST(test_humidity_max_threshold);
    RUN_TEST(test_dewpoint_margin_threshold);
    RUN_TEST(test_stale_data_suppresses_threshold_checks);
    RUN_TEST(test_rain_makes_unsafe);
    RUN_TEST(test_rain_checked_even_when_other_data_stale);
    RUN_TEST(test_rain_rule_disabled_or_sensor_absent);
    RUN_TEST(test_rain_sensor_required);
    RUN_TEST(test_environment_fault_blocks_humidity_rules);
    RUN_TEST(test_faulted_sensor_skips_its_threshold);
    RUN_TEST(test_safe_delay_filter);
    RUN_TEST(test_safe_delay_filter_survives_millis_wrap);
    RUN_TEST(test_unsafe_delay_off_is_immediate);
    RUN_TEST(test_unsafe_delay_ignores_a_brief_unsafe);
    RUN_TEST(test_unsafe_delay_reports_after_the_delay);
    RUN_TEST(test_unsafe_delay_resets_when_safe_again);
    RUN_TEST(test_unsafe_delay_bypassed_by_rain_override_and_shutdown);
    RUN_TEST(test_unsafe_delay_then_safe_delay);
    RUN_TEST(test_unsafe_delay_never_reports_safe_at_boot);
    RUN_TEST(test_unsafe_delay_survives_millis_wrap);
    RUN_TEST(test_wind_limits);
    RUN_TEST(test_wind_limit_without_sensor_is_unsafe);
    RUN_TEST(test_sky_fault_reasons_per_role);
    RUN_TEST(test_sky_limit_reasons_per_role);
    run_battery_rule_tests();
    return UNITY_END();
}
