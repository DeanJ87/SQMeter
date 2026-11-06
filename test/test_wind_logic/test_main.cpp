#include <unity.h>
#include "As5600.h"
#include "WindAggregator.h"

using namespace SQM;
using namespace SQM::Wind;

void setUp(void) {}
void tearDown(void) {}

void test_empty_aggregator(void)
{
    WindAggregator agg;
    float dir = -1.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, agg.speedMs());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, agg.gustMs());
    TEST_ASSERT_FALSE(agg.directionDeg(dir));
}

void test_speed_is_two_minute_mean(void)
{
    WindAggregator agg;
    for (int i = 0; i < 300; ++i)
        agg.addSample(10.0f, -1.0f); // old, outside the 2-min window
    for (int i = 0; i < 120; ++i)
        agg.addSample(4.0f, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, agg.speedMs());
}

void test_gust_is_max_three_second_mean(void)
{
    WindAggregator agg;
    for (int i = 0; i < 100; ++i)
        agg.addSample(2.0f, -1.0f);
    agg.addSample(9.0f, -1.0f);
    agg.addSample(12.0f, -1.0f);
    agg.addSample(9.0f, -1.0f);
    for (int i = 0; i < 100; ++i)
        agg.addSample(2.0f, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, agg.gustMs());
}

void test_gust_window_expires_after_ten_minutes(void)
{
    WindAggregator agg;
    for (int i = 0; i < 3; ++i)
        agg.addSample(20.0f, -1.0f);
    for (size_t i = 0; i < WindAggregator::HISTORY_SECONDS; ++i)
        agg.addSample(1.0f, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, agg.gustMs());
}

void test_direction_circular_mean_wraps_north(void)
{
    WindAggregator agg;
    agg.addSample(5.0f, 350.0f);
    agg.addSample(5.0f, 10.0f);
    float dir = -1.0f;
    TEST_ASSERT_TRUE(agg.directionDeg(dir));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, dir);
}

void test_direction_ignores_calm_and_missing_vane(void)
{
    WindAggregator agg;
    agg.addSample(0.0f, 90.0f);
    agg.addSample(3.0f, -1.0f);
    float dir = -1.0f;
    TEST_ASSERT_FALSE(agg.directionDeg(dir));
    agg.addSample(3.0f, 270.0f);
    TEST_ASSERT_TRUE(agg.directionDeg(dir));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 270.0f, dir);
}

void test_as5600_angle(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, As5600::angleDeg(0x00, 0x00));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, As5600::angleDeg(0x04, 0x00));
    TEST_ASSERT_EQUAL_FLOAT(180.0f, As5600::angleDeg(0x08, 0x00));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 359.912f, As5600::angleDeg(0x0F, 0xFF));
    // The unused top bits of the high byte are ignored
    TEST_ASSERT_EQUAL_FLOAT(90.0f, As5600::angleDeg(0xF4, 0x00));
}

void test_as5600_magnet_status(void)
{
    TEST_ASSERT_EQUAL(As5600::Magnet::None, As5600::magnetFromStatus(0x00));
    TEST_ASSERT_EQUAL(As5600::Magnet::None, As5600::magnetFromStatus(0x10)); // weak bit without a magnet
    TEST_ASSERT_EQUAL(As5600::Magnet::Ok, As5600::magnetFromStatus(0x20));
    TEST_ASSERT_EQUAL(As5600::Magnet::Weak, As5600::magnetFromStatus(0x30));
    TEST_ASSERT_EQUAL(As5600::Magnet::Strong, As5600::magnetFromStatus(0x28));
    TEST_ASSERT_TRUE(As5600::hasAngle(As5600::Magnet::Weak));
    TEST_ASSERT_FALSE(As5600::hasAngle(As5600::Magnet::None));
    TEST_ASSERT_FALSE(As5600::hasAngle(As5600::Magnet::NoAnswer));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_aggregator);
    RUN_TEST(test_speed_is_two_minute_mean);
    RUN_TEST(test_gust_is_max_three_second_mean);
    RUN_TEST(test_gust_window_expires_after_ten_minutes);
    RUN_TEST(test_direction_circular_mean_wraps_north);
    RUN_TEST(test_direction_ignores_calm_and_missing_vane);
    RUN_TEST(test_as5600_angle);
    RUN_TEST(test_as5600_magnet_status);
    return UNITY_END();
}
