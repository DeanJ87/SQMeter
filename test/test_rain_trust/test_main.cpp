#include "RainTrust.h"
#include "SafetyEvaluator.h"
#include <unity.h>

#include <string>
#include <vector>

// Rain needs cloud (specs/036-rain-needs-cloud): the classification, the
// latch, the verdict and reasons it produces, and replays of the November
// 2026 nights the spec's success criteria name.

using namespace SQM::Alpaca;

namespace
{
    // A fresh, healthy device with the rain sensor on and Rain needs cloud at 10%.
    SafetyInputs healthy(bool raining, float rate, float cloud)
    {
        SafetyInputs in;
        in.hasEverHadGoodData = true;
        in.secondsSinceLastGoodData = 2;
        in.rainSensorEnabled = true;
        in.rainSensorHealthy = true;
        in.raining = raining;
        in.rainRateMmPerHour = rate;
        in.skyCloudUsable = true;
        in.cloudCoverPercent = cloud;
        return in;
    }

    SafetyThresholds needsCloud(bool on = true, float level = 10.0f)
    {
        SafetyThresholds t;
        t.cloudCoverEnabled = false; // corroboration is independent of the cloud rule
        t.rainNeedsCloud = on;
        t.rainNeedsCloudPercent = level;
        return t;
    }

    // One safety pass, as the firmware and the demo run it.
    SafetyResult pass(SafetyInputs in, const SafetyThresholds &t, RainTrustLatch &latch)
    {
        applyRainTrust(in, t, latch);
        return evaluateSafety(in, t);
    }

    bool rainUnsafe(const SafetyResult &r)
    {
        return (r.reasonFlags & UnsafeRain) != 0;
    }

    std::string rainReason(const SafetyResult &r)
    {
        return r.unsafeReasons.empty() ? std::string() : r.unsafeReasons.front();
    }
} // namespace

void setUp(void) {}
void tearDown(void) {}

// --- classification ---

void test_classify_dry_whatever_the_sky(void)
{
    RainEvidence e;
    TEST_ASSERT_EQUAL(RainClass::Dry, classifyRain(e, {true, 10.0f}));
    e.latched = true;
    TEST_ASSERT_EQUAL(RainClass::Dry, classifyRain(e, {false, 10.0f}));
}

void test_classify_setting_off_is_unchecked(void)
{
    RainEvidence e{true, true, 0.0f, false};
    TEST_ASSERT_EQUAL(RainClass::Unchecked, classifyRain(e, {false, 10.0f}));
}

void test_classify_cloud_at_the_level_confirms_and_just_below_is_dew(void)
{
    TEST_ASSERT_EQUAL(RainClass::Cloud, classifyRain({true, true, 10.0f, false}, {true, 10.0f}));
    TEST_ASSERT_EQUAL(RainClass::DewSuspected, classifyRain({true, true, 9.9f, false}, {true, 10.0f}));
}

void test_classify_unusable_or_non_finite_sky_trusts_rain(void)
{
    TEST_ASSERT_EQUAL(RainClass::NoSkyReading, classifyRain({true, false, 0.0f, false}, {true, 10.0f}));
    TEST_ASSERT_EQUAL(RainClass::NoSkyReading, classifyRain({true, true, NAN, false}, {true, 10.0f}));
}

void test_classify_latched_rain_under_clear_sky_is_held(void)
{
    TEST_ASSERT_EQUAL(RainClass::Held, classifyRain({true, true, 0.0f, true}, {true, 10.0f}));
}

// --- the verdict ---

void test_setting_off_rain_is_unsafe_as_before(void)
{
    RainTrustLatch latch;
    const SafetyResult r = pass(healthy(true, 0.0f, 0.0f), needsCloud(false), latch);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_STRING("Rain detected", rainReason(r).c_str());
    TEST_ASSERT_FALSE(r.dewSuspected);
}

void test_setting_off_reason_states_the_rate(void)
{
    RainTrustLatch latch;
    const SafetyResult r = pass(healthy(true, 0.4f, 0.0f), needsCloud(false), latch);
    TEST_ASSERT_EQUAL_STRING("Rain 0.40 mm/h", rainReason(r).c_str());
}

void test_clear_healthy_sky_is_safe_with_dew_flag(void)
{
    RainTrustLatch latch;
    const SafetyResult r = pass(healthy(true, 0.12f, 0.0f), needsCloud(), latch);
    TEST_ASSERT_TRUE(r.isSafe);
    TEST_ASSERT_TRUE(r.dewSuspected);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.12f, r.rainRateMmPerHour);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.cloudCoverPercent);
}

void test_dew_flag_clears_when_rain_stops(void)
{
    RainTrustLatch latch;
    TEST_ASSERT_TRUE(pass(healthy(true, 0.1f, 0.0f), needsCloud(), latch).dewSuspected);
    const SafetyResult r = pass(healthy(false, 0.0f, 0.0f), needsCloud(), latch);
    TEST_ASSERT_FALSE(r.dewSuspected);
    TEST_ASSERT_TRUE(r.isSafe);
}

void test_cloud_at_level_is_unsafe_with_values_in_reason(void)
{
    RainTrustLatch latch;
    const SafetyResult r = pass(healthy(true, 0.4f, 35.0f), needsCloud(), latch);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_TRUE(rainUnsafe(r));
    TEST_ASSERT_FALSE(r.dewSuspected);
    TEST_ASSERT_EQUAL_STRING("Rain 0.40 mm/h with cloud 35% >= 10%", rainReason(r).c_str());
}

void test_cloud_rising_to_level_mid_dew_goes_unsafe_in_one_pass(void)
{
    RainTrustLatch latch;
    TEST_ASSERT_TRUE(pass(healthy(true, 0.1f, 5.0f), needsCloud(), latch).isSafe);
    const SafetyResult r = pass(healthy(true, 0.1f, 10.0f), needsCloud(), latch);
    TEST_ASSERT_TRUE(rainUnsafe(r));
    TEST_ASSERT_FALSE(r.dewSuspected);
}

void test_untrusted_sky_makes_rain_unsafe(void)
{
    // Disabled, missing, stale, faulted or uncomputable all reach the rule as
    // "not usable" (Core::safetyInputs, test_device_core).
    RainTrustLatch latch;
    SafetyInputs in = healthy(true, 0.3f, 0.0f);
    in.skyCloudUsable = false;
    const SafetyResult r = pass(in, needsCloud(), latch);
    TEST_ASSERT_TRUE(rainUnsafe(r));
    TEST_ASSERT_FALSE(r.dewSuspected);
    TEST_ASSERT_EQUAL_STRING("Rain 0.30 mm/h with no cloud reading", rainReason(r).c_str());
}

void test_sky_going_stale_during_dew_is_unsafe_next_pass(void)
{
    RainTrustLatch latch;
    TEST_ASSERT_TRUE(pass(healthy(true, 0.1f, 0.0f), needsCloud(), latch).isSafe);
    SafetyInputs in = healthy(true, 0.1f, 0.0f);
    in.skyCloudUsable = false;
    TEST_ASSERT_TRUE(rainUnsafe(pass(in, needsCloud(), latch)));
}

void test_latch_holds_across_a_clear_flicker_until_dry(void)
{
    RainTrustLatch latch;
    TEST_ASSERT_TRUE(rainUnsafe(pass(healthy(true, 1.2f, 40.0f), needsCloud(), latch)));
    // A gap in the cloud mid-shower: still unsafe.
    const SafetyResult gap = pass(healthy(true, 0.8f, 3.0f), needsCloud(), latch);
    TEST_ASSERT_TRUE(rainUnsafe(gap));
    TEST_ASSERT_FALSE(gap.dewSuspected);
    TEST_ASSERT_EQUAL_STRING("Rain 0.80 mm/h, held until dry", rainReason(gap).c_str());
    // The hold-off (raining, no rate) keeps it too.
    TEST_ASSERT_TRUE(rainUnsafe(pass(healthy(true, 0.0f, 3.0f), needsCloud(), latch)));
    // Dry: the latch ends; the next drops under a clear sky are dew.
    TEST_ASSERT_TRUE(pass(healthy(false, 0.0f, 3.0f), needsCloud(), latch).isSafe);
    TEST_ASSERT_TRUE(pass(healthy(true, 0.05f, 3.0f), needsCloud(), latch).dewSuspected);
}

void test_latch_also_holds_rain_trusted_for_an_untrusted_sky(void)
{
    RainTrustLatch latch;
    SafetyInputs in = healthy(true, 0.5f, 0.0f);
    in.skyCloudUsable = false;
    TEST_ASSERT_TRUE(rainUnsafe(pass(in, needsCloud(), latch)));
    // The sky sensor comes back with a clear reading while it still rains.
    TEST_ASSERT_TRUE(rainUnsafe(pass(healthy(true, 0.5f, 0.0f), needsCloud(), latch)));
}

void test_rain_rule_off_still_reports_dew(void)
{
    RainTrustLatch latch;
    SafetyThresholds t = needsCloud();
    t.rainUnsafeEnabled = false;
    const SafetyResult r = pass(healthy(true, 0.1f, 0.0f), t, latch);
    TEST_ASSERT_TRUE(r.isSafe);
    TEST_ASSERT_TRUE(r.dewSuspected);
    TEST_ASSERT_TRUE(pass(healthy(true, 0.1f, 50.0f), t, latch).isSafe); // rule off: never unsafe for rain
}

void test_rain_sensor_fault_rule_unchanged(void)
{
    RainTrustLatch latch;
    SafetyInputs in = healthy(true, 0.1f, 0.0f);
    in.rainSensorHealthy = false;
    const SafetyResult r = pass(in, needsCloud(), latch);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_TRUE((r.reasonFlags & UnsafeRainSensorFault) != 0);
    TEST_ASSERT_FALSE(rainUnsafe(r));
}

void test_holds_and_manual_unsafe_unchanged(void)
{
    RainTrustLatch latch;
    SafetyInputs in = healthy(true, 0.1f, 0.0f);
    in.holdReasons = {"Roof open"};
    SafetyThresholds t = needsCloud();
    t.manualOverrideUnsafe = true;
    const SafetyResult r = pass(in, t, latch);
    TEST_ASSERT_TRUE((r.reasonFlags & UnsafeExternalHold) != 0);
    TEST_ASSERT_TRUE((r.reasonFlags & UnsafeManualOverride) != 0);
    TEST_ASSERT_FALSE(rainUnsafe(r));
    TEST_ASSERT_TRUE(r.dewSuspected);
}

void test_corroboration_ignores_the_cloud_rule_limit(void)
{
    // Cloud rule on at 90%: 35% isn't unsafe for cloud, but it confirms rain at 10%.
    RainTrustLatch latch;
    SafetyThresholds t = needsCloud();
    t.cloudCoverEnabled = true;
    t.cloudCoverUnsafePercent = 90.0f;
    const SafetyResult r = pass(healthy(true, 0.4f, 35.0f), t, latch);
    TEST_ASSERT_EQUAL_UINT32(UnsafeRain, r.reasonFlags);
}

void test_unclassified_inputs_keep_rain_unsafe(void)
{
    // A caller that doesn't classify (the Alpaca simulator, older code) gets today's rule.
    SafetyInputs in = healthy(true, 0.1f, 0.0f);
    TEST_ASSERT_TRUE(rainUnsafe(evaluateSafety(in, needsCloud())));
}

// --- replays (SC-001, SC-002) ---

namespace
{
    struct Sample
    {
        bool raining;
        float rate;
        float cloud;
    };

    // Per pass: was it unsafe for rain, and was dew suspected.
    struct Outcome
    {
        std::vector<bool> unsafe;
        std::vector<bool> dew;
    };

    Outcome replay(const std::vector<Sample> &samples, bool settingOn)
    {
        RainTrustLatch latch;
        Outcome out;
        for (const Sample &s : samples)
        {
            const SafetyResult r = pass(healthy(s.raining, s.rate, s.cloud), needsCloud(settingOn), latch);
            out.unsafe.push_back(rainUnsafe(r));
            out.dew.push_back(r.dewSuspected);
        }
        return out;
    }

    int firstUnsafe(const Outcome &o)
    {
        for (size_t i = 0; i < o.unsafe.size(); ++i)
            if (o.unsafe[i])
                return static_cast<int>(i);
        return -1;
    }
} // namespace

void test_replay_20_nov_1943_dew_never_unsafe(void)
{
    // 19:43 onward: the RG-15 dews over and reports 0.02-0.17 mm/h from a 0%
    // sky, then holds off after the last reported drop.
    const std::vector<Sample> night = {
        {false, 0.0f, 0.0f},
        {true, 0.02f, 0.0f},
        {true, 0.05f, 0.0f},
        {true, 0.11f, 0.0f},
        {true, 0.17f, 0.0f},
        {true, 0.09f, 0.0f},
        {true, 0.04f, 0.0f},
        {true, 0.02f, 0.0f},
        {true, 0.0f, 0.0f},
        {true, 0.0f, 0.0f},
        {false, 0.0f, 0.0f},
    };
    const Outcome on = replay(night, true);
    for (size_t i = 0; i < night.size(); ++i)
    {
        TEST_ASSERT_FALSE(on.unsafe[i]);
        TEST_ASSERT_EQUAL(night[i].raining, on.dew[i]); // flagged for the duration
    }
    TEST_ASSERT_EQUAL(1, firstUnsafe(replay(night, false))); // what parked the scope
}

void test_replay_22_nov_0549_shower_unsafe_no_later(void)
{
    // 05:49: cloud builds to 35-60% and it rains, with a brief clear gap.
    const std::vector<Sample> morning = {
        {false, 0.0f, 4.0f},
        {false, 0.0f, 22.0f},
        {true, 0.3f, 35.0f},
        {true, 1.4f, 58.0f},
        {true, 2.1f, 60.0f},
        {true, 0.9f, 8.0f},
        {true, 0.4f, 41.0f},
        {true, 0.0f, 30.0f},
        {true, 0.0f, 6.0f},
        {false, 0.0f, 5.0f},
    };
    const Outcome on = replay(morning, true);
    const Outcome off = replay(morning, false);
    TEST_ASSERT_EQUAL(firstUnsafe(off), firstUnsafe(on));
    for (size_t i = 0; i < morning.size(); ++i)
    {
        TEST_ASSERT_EQUAL(off.unsafe[i], on.unsafe[i]); // the clear gap and the hold-off stay unsafe
        TEST_ASSERT_FALSE(on.dew[i]);
    }
}

void test_untrusted_sky_every_rain_input_unsafe(void)
{
    // SC-003: whatever the reported cloud, rain with an unusable sky counts.
    const float clouds[] = {0.0f, 5.0f, 9.9f, 10.0f, 100.0f};
    const float rates[] = {0.0f, 0.02f, 0.4f, 5.0f};
    for (float cloud : clouds)
        for (float rate : rates)
        {
            RainTrustLatch latch;
            SafetyInputs in = healthy(true, rate, cloud);
            in.skyCloudUsable = false;
            const SafetyResult r = pass(in, needsCloud(), latch);
            TEST_ASSERT_TRUE(rainUnsafe(r));
            TEST_ASSERT_FALSE(r.dewSuspected);
        }
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_classify_dry_whatever_the_sky);
    RUN_TEST(test_classify_setting_off_is_unchecked);
    RUN_TEST(test_classify_cloud_at_the_level_confirms_and_just_below_is_dew);
    RUN_TEST(test_classify_unusable_or_non_finite_sky_trusts_rain);
    RUN_TEST(test_classify_latched_rain_under_clear_sky_is_held);
    RUN_TEST(test_setting_off_rain_is_unsafe_as_before);
    RUN_TEST(test_setting_off_reason_states_the_rate);
    RUN_TEST(test_clear_healthy_sky_is_safe_with_dew_flag);
    RUN_TEST(test_dew_flag_clears_when_rain_stops);
    RUN_TEST(test_cloud_at_level_is_unsafe_with_values_in_reason);
    RUN_TEST(test_cloud_rising_to_level_mid_dew_goes_unsafe_in_one_pass);
    RUN_TEST(test_untrusted_sky_makes_rain_unsafe);
    RUN_TEST(test_sky_going_stale_during_dew_is_unsafe_next_pass);
    RUN_TEST(test_latch_holds_across_a_clear_flicker_until_dry);
    RUN_TEST(test_latch_also_holds_rain_trusted_for_an_untrusted_sky);
    RUN_TEST(test_rain_rule_off_still_reports_dew);
    RUN_TEST(test_rain_sensor_fault_rule_unchanged);
    RUN_TEST(test_holds_and_manual_unsafe_unchanged);
    RUN_TEST(test_corroboration_ignores_the_cloud_rule_limit);
    RUN_TEST(test_unclassified_inputs_keep_rain_unsafe);
    RUN_TEST(test_replay_20_nov_1943_dew_never_unsafe);
    RUN_TEST(test_replay_22_nov_0549_shower_unsafe_no_later);
    RUN_TEST(test_untrusted_sky_every_rain_input_unsafe);
    return UNITY_END();
}
