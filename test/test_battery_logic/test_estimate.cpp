#include <unity.h>

#include "BatteryLogic.h"

// Time estimates (specs/032-battery research D4).

using namespace SQM::Battery;

namespace
{
    EstimateInput input(float percent, float rate, ChargeState state, uint16_t capacity = 0)
    {
        EstimateInput in;
        in.valid = true;
        in.percent = percent;
        in.rateValid = true;
        in.rate = rate;
        in.state = state;
        in.capacityMah = capacity;
        return in;
    }
} // namespace

void test_time_to_empty_and_full(void)
{
    // 82.4 % at -6.2 %/h: 13.29 h = 797 min.
    Estimate e = estimate(input(82.4f, -6.2f, ChargeState::Discharging));
    TEST_ASSERT_TRUE(e.timeToEmptyMinutes.has_value());
    TEST_ASSERT_EQUAL_UINT32(797, *e.timeToEmptyMinutes);
    TEST_ASSERT_FALSE(e.timeToFullMinutes.has_value());
    TEST_ASSERT_FALSE(e.currentMa.has_value());

    e = estimate(input(40.0f, 12.0f, ChargeState::Charging));
    TEST_ASSERT_FALSE(e.timeToEmptyMinutes.has_value());
    TEST_ASSERT_EQUAL_UINT32(300, *e.timeToFullMinutes);
}

void test_no_estimate_without_data(void)
{
    EstimateInput in = input(50.0f, -5.0f, ChargeState::Discharging);
    in.valid = false; // estimating
    TEST_ASSERT_FALSE(estimate(in).timeToEmptyMinutes.has_value());
    in = input(50.0f, -5.0f, ChargeState::Discharging);
    in.rateValid = false;
    TEST_ASSERT_FALSE(estimate(in).timeToEmptyMinutes.has_value());
    // Near-zero rate.
    TEST_ASSERT_FALSE(estimate(input(50.0f, -0.4f, ChargeState::Discharging)).timeToEmptyMinutes.has_value());
    TEST_ASSERT_FALSE(estimate(input(50.0f, 0.4f, ChargeState::Charging)).timeToFullMinutes.has_value());
    // Other states.
    for (ChargeState s : {ChargeState::Charged, ChargeState::Fault, ChargeState::NoBattery, ChargeState::Unknown})
    {
        const Estimate e = estimate(input(50.0f, -5.0f, s));
        TEST_ASSERT_FALSE(e.timeToEmptyMinutes.has_value());
        TEST_ASSERT_FALSE(e.timeToFullMinutes.has_value());
    }
    // Rate against the state.
    TEST_ASSERT_FALSE(estimate(input(50.0f, 5.0f, ChargeState::Discharging)).timeToEmptyMinutes.has_value());
}

void test_estimate_capped_at_99_hours(void)
{
    const Estimate e = estimate(input(100.0f, -0.6f, ChargeState::Discharging));
    TEST_ASSERT_EQUAL_UINT32(5940, *e.timeToEmptyMinutes);
    TEST_ASSERT_EQUAL_UINT32(5940, MAX_ESTIMATE_MINUTES);
}

void test_capacity_bounds_time_to_full_and_gives_current(void)
{
    // 7000 mAh from 40 %: 4200 mAh at 833 mA is at least 5.04 h, longer than the
    // rate says (60 %/30 %/h = 2 h).
    Estimate e = estimate(input(40.0f, 30.0f, ChargeState::Charging, 7000));
    TEST_ASSERT_EQUAL_UINT32(303, *e.timeToFullMinutes);
    TEST_ASSERT_TRUE(e.currentMa.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2100.0f, *e.currentMa);
    // Slower than the charger's limit: the rate decides.
    e = estimate(input(40.0f, 6.0f, ChargeState::Charging, 7000));
    TEST_ASSERT_EQUAL_UINT32(600, *e.timeToFullMinutes);
    // Discharging current is negative.
    e = estimate(input(50.0f, -6.0f, ChargeState::Discharging, 7000));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -420.0f, *e.currentMa);
    TEST_ASSERT_FALSE(estimate(input(50.0f, -6.0f, ChargeState::Discharging)).currentMa.has_value());
}

void run_estimate_tests(void)
{
    RUN_TEST(test_time_to_empty_and_full);
    RUN_TEST(test_no_estimate_without_data);
    RUN_TEST(test_estimate_capped_at_99_hours);
    RUN_TEST(test_capacity_bounds_time_to_full_and_gives_current);
}
