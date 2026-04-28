#include <unity.h>
#include <cmath>
#include "calculations/SkyQuality.h"

using namespace SQM;

void setUp(void) {}
void tearDown(void) {}

// --- luxToSQM tests ---

void test_sqm_formula_lux_1(void) {
    // lux=1.0 → SQM = 12.6 - 2.5*log10(1.0) = 12.6
    float sqm = SkyQuality::luxToSQM(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.6f, sqm);
}

void test_sqm_formula_lux_0001(void) {
    // lux=0.001 → SQM = 12.6 - 2.5*log10(0.001) = 12.6 + 7.5 = 20.1
    float sqm = SkyQuality::luxToSQM(0.001f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 20.1f, sqm);
}

void test_sqm_formula_lux_00001(void) {
    // lux=0.0001 → SQM = 12.6 - 2.5*log10(0.0001) = 12.6 + 10.0 = 22.6
    float sqm = SkyQuality::luxToSQM(0.0001f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 22.6f, sqm);
}

void test_sqm_min_lux_clamp(void) {
    // lux below MIN_LUX (0.0001) should be clamped; luxToSQM(0.00001) == luxToSQM(0.0001) = 22.6
    float sqm_clamped = SkyQuality::luxToSQM(0.00001f);
    float sqm_min = SkyQuality::luxToSQM(0.0001f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, sqm_min, sqm_clamped);
}

// --- Bortle scale boundary tests ---

void test_bortle_class_1_excellent(void) {
    // SQM >= 21.99 → Bortle 1 (Excellent dark-sky site)
    // lux=0.0001 → SQM=22.6
    float bortle = SkyQuality::sqmToBortle(22.6f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, bortle);
}

void test_bortle_class_2_typical_dark(void) {
    // SQM = 21.95 → Bortle 2 (Typical truly dark site)
    float bortle = SkyQuality::sqmToBortle(21.95f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, bortle);
}

void test_bortle_class_3_rural(void) {
    // SQM = 21.75 → Bortle 3 (Rural sky)
    float bortle = SkyQuality::sqmToBortle(21.75f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, bortle);
}

void test_bortle_class_5_suburban(void) {
    // SQM = 19.0 → Bortle 5 (Suburban sky), 19.5 > 19.0 >= 18.94 → actually 6
    // 19.0 < 19.50 but >= 18.94 → Bortle 6
    float bortle = SkyQuality::sqmToBortle(19.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, bortle);
}

void test_bortle_class_9_inner_city(void) {
    // SQM < 17.0 → Bortle 9 (Inner city)
    // lux=1.0 → SQM=12.6 < 17.0
    float bortle = SkyQuality::sqmToBortle(12.6f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.0f, bortle);
}

// --- NELM tests ---

void test_nelm_bright_sky_returns_zero(void) {
    // SQM < 15 → NELM = 0 (no stars visible)
    float nelm = SkyQuality::sqmToNELM(12.6f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, nelm);
}

void test_nelm_dark_sky(void) {
    // SQM=22.6 → NELM ≈ 6.88 (excellent dark site)
    float nelm = SkyQuality::sqmToNELM(22.6f);
    TEST_ASSERT_GREATER_THAN(6.0f, nelm);
    TEST_ASSERT_LESS_THAN(8.0f, nelm);
}

void test_nelm_moderate_sky(void) {
    // SQM=20.1 → NELM ≈ 5.56 (suburban/rural transition)
    float nelm = SkyQuality::sqmToNELM(20.1f);
    TEST_ASSERT_GREATER_THAN(4.0f, nelm);
    TEST_ASSERT_LESS_THAN(7.0f, nelm);
}

// --- Full calculate() pipeline tests ---

void test_calculate_bright_sky(void) {
    // High lux = bright (city) sky → low SQM, high Bortle, zero NELM
    SkyQualityMetrics m = SkyQuality::calculate(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 12.6f, m.sqm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.0f, m.bortle);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, m.nelm);
    TEST_ASSERT_TRUE(m.isValid());
}

void test_calculate_dark_sky(void) {
    // Very low lux = dark sky → high SQM, low Bortle
    SkyQualityMetrics m = SkyQuality::calculate(0.0001f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 22.6f, m.sqm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, m.bortle);
    TEST_ASSERT_GREATER_THAN(6.0f, m.nelm);
    TEST_ASSERT_TRUE(m.isValid());
}

void test_calculate_lux_stored(void) {
    // calculate() clamps input lux to MIN_LUX and stores it
    SkyQualityMetrics m = SkyQuality::calculate(0.00001f);
    // Clamped to MIN_LUX = 0.0001
    TEST_ASSERT_FLOAT_WITHIN(0.00001f, 0.0001f, m.lux);
}

// --- Bortle description tests ---

void test_bortle_description_class1(void) {
    const char *desc = SkyQuality::getBortleDescription(1.0f);
    TEST_ASSERT_EQUAL_STRING("Excellent dark-sky site", desc);
}

void test_bortle_description_class9(void) {
    const char *desc = SkyQuality::getBortleDescription(9.0f);
    TEST_ASSERT_EQUAL_STRING("Inner-city sky", desc);
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_sqm_formula_lux_1);
    RUN_TEST(test_sqm_formula_lux_0001);
    RUN_TEST(test_sqm_formula_lux_00001);
    RUN_TEST(test_sqm_min_lux_clamp);
    RUN_TEST(test_bortle_class_1_excellent);
    RUN_TEST(test_bortle_class_2_typical_dark);
    RUN_TEST(test_bortle_class_3_rural);
    RUN_TEST(test_bortle_class_5_suburban);
    RUN_TEST(test_bortle_class_9_inner_city);
    RUN_TEST(test_nelm_bright_sky_returns_zero);
    RUN_TEST(test_nelm_dark_sky);
    RUN_TEST(test_nelm_moderate_sky);
    RUN_TEST(test_calculate_bright_sky);
    RUN_TEST(test_calculate_dark_sky);
    RUN_TEST(test_calculate_lux_stored);
    RUN_TEST(test_bortle_description_class1);
    RUN_TEST(test_bortle_description_class9);
    UNITY_END();
}
void loop() {}
#else
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sqm_formula_lux_1);
    RUN_TEST(test_sqm_formula_lux_0001);
    RUN_TEST(test_sqm_formula_lux_00001);
    RUN_TEST(test_sqm_min_lux_clamp);
    RUN_TEST(test_bortle_class_1_excellent);
    RUN_TEST(test_bortle_class_2_typical_dark);
    RUN_TEST(test_bortle_class_3_rural);
    RUN_TEST(test_bortle_class_5_suburban);
    RUN_TEST(test_bortle_class_9_inner_city);
    RUN_TEST(test_nelm_bright_sky_returns_zero);
    RUN_TEST(test_nelm_dark_sky);
    RUN_TEST(test_nelm_moderate_sky);
    RUN_TEST(test_calculate_bright_sky);
    RUN_TEST(test_calculate_dark_sky);
    RUN_TEST(test_calculate_lux_stored);
    RUN_TEST(test_bortle_description_class1);
    RUN_TEST(test_bortle_description_class9);
    return UNITY_END();
}
#endif
