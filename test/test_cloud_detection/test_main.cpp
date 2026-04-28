#include <unity.h>
#include "calculations/CloudDetection.h"

using namespace SQM;

void setUp(void) {}
void tearDown(void) {}

void test_clear_sky(void) {
    // skyTemp=-20, ambientTemp=22, humidity=50
    // delta = -42, correction = 0.375, corrected = -42.375 → CLEAR
    auto result = CloudDetection::calculate(-20.0f, 22.0f, 50.0f);
    TEST_ASSERT_EQUAL(CloudCondition::CLEAR, result.condition);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, result.cloudCoverPercent);
}

void test_overcast_sky(void) {
    // skyTemp=20, ambientTemp=22, humidity=50
    // delta = -2, correction = 0.375, corrected = -2.375 → OVERCAST (>= -3)
    auto result = CloudDetection::calculate(20.0f, 22.0f, 50.0f);
    TEST_ASSERT_EQUAL(CloudCondition::OVERCAST, result.condition);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, result.cloudCoverPercent);
}

void test_cloudy_sky(void) {
    // skyTemp=14, ambientTemp=22, humidity=0
    // delta = -8, correction = 0, corrected = -8 → CLOUDY (-13 <= -8 < -3)
    auto result = CloudDetection::calculate(14.0f, 22.0f, 0.0f);
    TEST_ASSERT_EQUAL(CloudCondition::CLOUDY, result.condition);
    TEST_ASSERT_GREATER_THAN(0.0f, result.cloudCoverPercent);
    TEST_ASSERT_LESS_THAN(100.0f, result.cloudCoverPercent);
}

void test_humidity_correction(void) {
    // skyTemp=-18, ambientTemp=22, humidity=100
    // delta = -40, correction = (0.75/100)*100 = 0.75, corrected = -40.75
    auto result = CloudDetection::calculate(-18.0f, 22.0f, 100.0f);
    float expectedDelta = -18.0f - 22.0f;                        // -40.0
    float expectedCorrected = expectedDelta - (0.75f / 100.0f * 100.0f); // -40.75
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expectedDelta, result.temperatureDelta);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expectedCorrected, result.correctedDelta);
}

void test_cloud_cover_clamped(void) {
    // Very clear sky: corrected well below -13 → 0%
    auto clear = CloudDetection::calculate(-40.0f, 22.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, clear.cloudCoverPercent);

    // Very overcast sky: corrected well above -3 → 100%
    auto overcast = CloudDetection::calculate(30.0f, 22.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, overcast.cloudCoverPercent);
}

void test_temperature_delta_calculation(void) {
    // skyTemp=-5, ambientTemp=20 → delta = -25
    auto result = CloudDetection::calculate(-5.0f, 20.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -25.0f, result.temperatureDelta);
}

void test_clear_threshold_boundary(void) {
    // correctedDelta just at the CLEAR boundary: delta=-13 exactly (no humidity)
    // skyTemp = ambientTemp + (-13) = 20 + (-13) = 7, ambientTemp=20, humidity=0
    // delta = 7 - 20 = -13, corrected = -13 → classifyCondition(-13) = CLOUDY (not < -13)
    auto result = CloudDetection::calculate(7.0f, 20.0f, 0.0f);
    TEST_ASSERT_EQUAL(CloudCondition::CLOUDY, result.condition);
}

void test_condition_description_clear(void) {
    auto result = CloudDetection::calculate(-20.0f, 22.0f, 0.0f);
    TEST_ASSERT_EQUAL_STRING("Clear", result.description);
}

void test_condition_description_overcast(void) {
    auto result = CloudDetection::calculate(20.0f, 22.0f, 0.0f);
    TEST_ASSERT_EQUAL_STRING("Overcast", result.description);
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_clear_sky);
    RUN_TEST(test_overcast_sky);
    RUN_TEST(test_cloudy_sky);
    RUN_TEST(test_humidity_correction);
    RUN_TEST(test_cloud_cover_clamped);
    RUN_TEST(test_temperature_delta_calculation);
    RUN_TEST(test_clear_threshold_boundary);
    RUN_TEST(test_condition_description_clear);
    RUN_TEST(test_condition_description_overcast);
    UNITY_END();
}
void loop() {}
#else
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clear_sky);
    RUN_TEST(test_overcast_sky);
    RUN_TEST(test_cloudy_sky);
    RUN_TEST(test_humidity_correction);
    RUN_TEST(test_cloud_cover_clamped);
    RUN_TEST(test_temperature_delta_calculation);
    RUN_TEST(test_clear_threshold_boundary);
    RUN_TEST(test_condition_description_clear);
    RUN_TEST(test_condition_description_overcast);
    return UNITY_END();
}
#endif
