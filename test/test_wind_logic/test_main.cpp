#include <unity.h>
#include "WindAggregator.h"

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

void test_vane_lookup(void)
{
    float dir = -1.0f;
    // 10k pull-up: 120k (270 deg) -> 120/130
    TEST_ASSERT_TRUE(vaneDirectionFromRatio(120.0f / 130.0f, 10000.0f, dir));
    TEST_ASSERT_EQUAL_FLOAT(270.0f, dir);
    // 3.9k (180 deg)
    TEST_ASSERT_TRUE(vaneDirectionFromRatio(3.9f / 13.9f, 10000.0f, dir));
    TEST_ASSERT_EQUAL_FLOAT(180.0f, dir);
    // Open circuit reads ~full scale - not a vane position
    TEST_ASSERT_FALSE(vaneDirectionFromRatio(0.995f, 10000.0f, dir));
    // Short to ground
    TEST_ASSERT_FALSE(vaneDirectionFromRatio(0.0f, 10000.0f, dir));
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
    RUN_TEST(test_vane_lookup);
    return UNITY_END();
}
