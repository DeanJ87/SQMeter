#include "SafetyEvaluator.h"
#include <unity.h>

#include <string>

// The battery safety rule and the shutdown reason (specs/032-battery,
// contracts/integrations.md).

using namespace SQM::Alpaca;

namespace
{
    SafetyInputs fresh()
    {
        SafetyInputs in;
        in.hasEverHadGoodData = true;
        in.secondsSinceLastGoodData = 5;
        in.cloudCoverPercent = 10.0f;
        return in;
    }

    SafetyThresholds ruleOn(float limit = 10.0f)
    {
        SafetyThresholds t;
        t.batteryEnabled = true;
        t.batteryMinPercent = limit;
        return t;
    }

    bool hasReason(const SafetyResult &r, const char *reason)
    {
        for (const std::string &s : r.unsafeReasons)
            if (s == reason)
                return true;
        return false;
    }
} // namespace

void test_battery_rule_off_gives_no_battery_reason(void)
{
    SafetyInputs in = fresh();
    in.batteryValid = false;
    in.batteryPercent = 2.0f;
    const SafetyResult r = evaluateSafety(in, SafetyThresholds{});
    TEST_ASSERT_TRUE(r.isSafe);
}

void test_battery_rule_without_data_is_unsafe(void)
{
    SafetyInputs in = fresh(); // no gauge, estimating or stale: not valid
    SafetyResult r = evaluateSafety(in, ruleOn());
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UnsafeBatteryData, r.reasonFlags);
    TEST_ASSERT_EQUAL_UINT32(1u << 15, UnsafeBatteryData);
    TEST_ASSERT_TRUE(hasReason(r, "Battery data missing"));
}

void test_battery_below_limit_is_unsafe_with_value_and_limit(void)
{
    SafetyInputs in = fresh();
    in.batteryValid = true;
    in.batteryPercent = 8.9f;
    SafetyResult r = evaluateSafety(in, ruleOn(10.0f));
    TEST_ASSERT_EQUAL_UINT32(UnsafeBatteryLow, r.reasonFlags);
    TEST_ASSERT_EQUAL_UINT32(1u << 14, UnsafeBatteryLow);
    TEST_ASSERT_TRUE(hasReason(r, "Battery 8% < 10%"));

    in.batteryPercent = 10.0f;
    TEST_ASSERT_TRUE(evaluateSafety(in, ruleOn(10.0f)).isSafe);
}

void test_battery_checked_when_everything_else_is_stale(void)
{
    SafetyInputs in;
    in.hasEverHadGoodData = true;
    in.secondsSinceLastGoodData = 600; // stale
    in.batteryValid = true;
    in.batteryPercent = 5.0f;
    in.rainSensorEnabled = true;
    in.rainSensorHealthy = true;
    in.raining = true;
    const SafetyResult r = evaluateSafety(in, ruleOn());
    TEST_ASSERT_TRUE((r.reasonFlags & UnsafeBatteryLow) != 0);
    TEST_ASSERT_TRUE((r.reasonFlags & UnsafeRain) != 0);
    TEST_ASSERT_TRUE((r.reasonFlags & UnsafeStaleData) != 0);
}

void test_charging_below_the_limit_stays_unsafe(void)
{
    // The rule measures charge, not whether it is charging: the input has no
    // charging state, so a charging pack at 6% reads exactly like a draining one.
    SafetyInputs in = fresh();
    in.batteryValid = true;
    in.batteryPercent = 6.0f;
    TEST_ASSERT_EQUAL_UINT32(UnsafeBatteryLow, evaluateSafety(in, ruleOn()).reasonFlags);
}

void test_shutdown_pending_is_unsafe_with_the_rule_off(void)
{
    SafetyInputs in = fresh();
    in.batteryValid = true;
    in.batteryPercent = 4.6f;
    in.shutdownPending = true;
    in.shutdownPercent = 5.0f;
    SafetyResult r = evaluateSafety(in, SafetyThresholds{});
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UnsafeShutdown, r.reasonFlags);
    TEST_ASSERT_EQUAL_UINT32(1u << 16, UnsafeShutdown);
    TEST_ASSERT_TRUE(hasReason(r, "Shutting down: battery 4% < 5%"));

    // With the rule on too, both reasons.
    r = evaluateSafety(in, ruleOn());
    TEST_ASSERT_EQUAL_UINT32(UnsafeShutdown | UnsafeBatteryLow, r.reasonFlags);
}

void run_battery_rule_tests(void)
{
    RUN_TEST(test_battery_rule_off_gives_no_battery_reason);
    RUN_TEST(test_battery_rule_without_data_is_unsafe);
    RUN_TEST(test_battery_below_limit_is_unsafe_with_value_and_limit);
    RUN_TEST(test_battery_checked_when_everything_else_is_stale);
    RUN_TEST(test_charging_below_the_limit_stays_unsafe);
    RUN_TEST(test_shutdown_pending_is_unsafe_with_the_rule_off);
}
