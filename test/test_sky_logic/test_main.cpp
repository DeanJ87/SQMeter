#include <unity.h>

#include <cmath>

#include "calculations/CloudDetection.h"
#include "calculations/Dewpoint.h"
#include "calculations/SkyQuality.h"

using namespace SQM;

void setUp() {}
void tearDown() {}

void test_lux_to_sqm()
{
    // MPSAS = 12.6 - 2.5 log10(lux)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.6f, SkyQuality::luxToSQM(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.6f, SkyQuality::luxToSQM(0.0001f));
    // Below the floor (incl. 0 and negative) clamps to 0.0001 lux.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.6f, SkyQuality::luxToSQM(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.6f, SkyQuality::luxToSQM(-1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.6f, SkyQuality::calculate(0.0f).sqm);
}

void test_bortle_boundaries()
{
    struct
    {
        float atLeast;
        float bortle;
    } const classes[] = {
        {21.99f, 1},
        {21.89f, 2},
        {21.69f, 3},
        {20.49f, 4},
        {19.50f, 5},
        {18.94f, 6},
        {18.38f, 7},
        {17.00f, 8},
    };
    for (const auto &c : classes)
    {
        TEST_ASSERT_EQUAL_FLOAT(c.bortle, SkyQuality::sqmToBortle(c.atLeast));
        TEST_ASSERT_EQUAL_FLOAT(c.bortle + 1, SkyQuality::sqmToBortle(c.atLeast - 0.001f));
    }
    TEST_ASSERT_EQUAL_FLOAT(1, SkyQuality::sqmToBortle(23.0f));
    TEST_ASSERT_EQUAL_FLOAT(9, SkyQuality::sqmToBortle(10.0f));
}

void test_bortle_descriptions()
{
    TEST_ASSERT_EQUAL_STRING("Excellent dark-sky site", SkyQuality::getBortleDescription(1));
    TEST_ASSERT_EQUAL_STRING("Inner-city sky", SkyQuality::getBortleDescription(9));
    TEST_ASSERT_EQUAL_STRING("Unknown", SkyQuality::getBortleDescription(0));
}

void test_nelm()
{
    // Unihedron: NELM = 7.93 - 5 log10(10^(4.316 - SQM/5) + 1)
    const float sqm = 21.5f;
    const float expected = 7.93f - 5.0f * std::log10(std::pow(10.0f, 4.316f - sqm / 5.0f) + 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, SkyQuality::sqmToNELM(sqm));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 6.4f, SkyQuality::sqmToNELM(21.5f));
    // Below SQM 15 no stars are visible: 0.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, SkyQuality::sqmToNELM(14.99f));
    TEST_ASSERT_TRUE(SkyQuality::sqmToNELM(15.0f) >= 0.0f);
}

void test_humidity_correction()
{
    // corrected = delta - (k1/100) * RH, RH clamped to 0..100
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.3975f, CloudDetection::applyHumidityCorrection(-20.0f, 53.0f, 0.75f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.75f, CloudDetection::applyHumidityCorrection(-20.0f, 150.0f, 0.75f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.0f, CloudDetection::applyHumidityCorrection(-20.0f, -5.0f, 0.75f));
}

void test_cloud_thresholds()
{
    const float clear = -13.0f, cloudy = -3.0f;
    // Below the clear threshold: clear, 0%.
    TEST_ASSERT_EQUAL(static_cast<int>(CloudCondition::CLEAR), static_cast<int>(CloudDetection::classifyCondition(-13.01f, clear, cloudy)));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CloudDetection::estimateCloudCover(-13.01f, clear, cloudy));
    // At the clear threshold: cloudy, 0%.
    TEST_ASSERT_EQUAL(static_cast<int>(CloudCondition::CLOUDY), static_cast<int>(CloudDetection::classifyCondition(-13.0f, clear, cloudy)));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CloudDetection::estimateCloudCover(-13.0f, clear, cloudy));
    // Midway: 50%.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, CloudDetection::estimateCloudCover(-8.0f, clear, cloudy));
    // Just below the cloudy threshold: cloudy; at it: overcast, 100%.
    TEST_ASSERT_EQUAL(static_cast<int>(CloudCondition::CLOUDY), static_cast<int>(CloudDetection::classifyCondition(-3.01f, clear, cloudy)));
    TEST_ASSERT_EQUAL(
        static_cast<int>(CloudCondition::OVERCAST), static_cast<int>(CloudDetection::classifyCondition(-3.0f, clear, cloudy)));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, CloudDetection::estimateCloudCover(-3.0f, clear, cloudy));
}

void test_cloud_calculate()
{
    // Sky -25, ambient 10, 53% RH, k1 0.75: delta -35, corrected -35.3975 -> clear.
    const CloudMetrics m = CloudDetection::calculate(-25.0f, 10.0f, 53.0f, -13.0f, -3.0f, 0.75f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -35.0f, m.temperatureDelta);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -35.3975f, m.correctedDelta);
    TEST_ASSERT_EQUAL(static_cast<int>(CloudCondition::CLEAR), static_cast<int>(m.condition));
    TEST_ASSERT_EQUAL_STRING("Clear", m.description);

    const CloudMetrics overcast = CloudDetection::calculate(8.0f, 10.0f, 90.0f, -13.0f, -3.0f, 0.75f);
    TEST_ASSERT_EQUAL_STRING("Overcast", overcast.description);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, overcast.cloudCoverPercent);
}

void test_dewpoint()
{
    // 20 °C at 50% -> about 9.26 °C; saturated air: dew point = temperature.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 9.26f, dewpointMagnus(20.0f, 50.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, dewpointMagnus(10.0f, 100.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -9.2f, dewpointMagnus(0.0f, 50.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dewpointMagnus(20.0f, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dewpointMagnus(20.0f, 101.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dewpointMagnus(20.0f, NAN));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_lux_to_sqm);
    RUN_TEST(test_bortle_boundaries);
    RUN_TEST(test_bortle_descriptions);
    RUN_TEST(test_nelm);
    RUN_TEST(test_humidity_correction);
    RUN_TEST(test_cloud_thresholds);
    RUN_TEST(test_cloud_calculate);
    RUN_TEST(test_dewpoint);
    return UNITY_END();
}
