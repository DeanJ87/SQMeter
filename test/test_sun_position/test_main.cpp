#include <unity.h>
#include "SunPosition.h"

using namespace SQM::Astro;

void setUp(void) {}
void tearDown(void) {}

// Reference values: noon elevation = 90 - |lat - declination|, midnight = lat + dec - 90.
void test_london_summer_solstice_noon(void)
{
    // 2024-06-20 20:51 UTC solstice; 2024-06-21 12:02 UTC is London's solar noon
    TEST_ASSERT_FLOAT_WITHIN(0.5, 61.96, sunElevationDeg(1718971320, 51.48, 0.0));
}

void test_london_winter_solstice_noon_and_midnight(void)
{
    // 2024-12-21 11:58 UTC (solar noon) and 2024-12-21 23:58 UTC
    TEST_ASSERT_FLOAT_WITHIN(0.5, 15.08, sunElevationDeg(1734782280, 51.48, 0.0));
    TEST_ASSERT_FLOAT_WITHIN(0.5, -61.96, sunElevationDeg(1734825480, 51.48, 0.0));
}

void test_longitude_shifts_local_noon(void)
{
    // Equinox 2024-03-20: near-overhead at the equator around 12:07 UTC at 0°,
    // and 6 hours later at 90°W.
    TEST_ASSERT_FLOAT_WITHIN(1.0, 89.5, sunElevationDeg(1710936420, 0.0, 0.0));
    TEST_ASSERT_FLOAT_WITHIN(1.0, 89.5, sunElevationDeg(1710936420 + 6 * 3600, 0.0, -90.0));
}

void test_southern_hemisphere(void)
{
    // Sydney (-33.87, 151.21) at local solar noon on the December solstice:
    // 90 - |-33.87 - (-23.44)| = 79.57
    TEST_ASSERT_FLOAT_WITHIN(0.6, 79.57, sunElevationDeg(1734745980, -33.87, 151.21));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_london_summer_solstice_noon);
    RUN_TEST(test_london_winter_solstice_noon_and_midnight);
    RUN_TEST(test_longitude_shifts_local_noon);
    RUN_TEST(test_southern_hemisphere);
    return UNITY_END();
}
