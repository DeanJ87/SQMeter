#include <unity.h>
#include <cstring>
#include "SafetyEvaluator.h"
#include "ObservingConditionsMapper.h"
#include "AlpacaDiscovery.h"

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

void test_observing_conditions_maps_known_property(void)
{
    ObservingConditionsSnapshot snap;
    snap.dataValid = true;
    snap.skyQualityMagArcsec2 = 21.3f;

    PropertyResult r = getObservingConditionsProperty("SkyQuality", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(21.3f, r.value);
}

void test_observing_conditions_case_insensitive(void)
{
    ObservingConditionsSnapshot snap;
    snap.dataValid = true;
    snap.temperatureC = 12.5f;

    PropertyResult r = getObservingConditionsProperty("TEMPERATURE", snap);
    TEST_ASSERT_TRUE(r.ok);
}

void test_observing_conditions_not_implemented_property(void)
{
    ObservingConditionsSnapshot snap;
    snap.dataValid = true;

    PropertyResult r = getObservingConditionsProperty("windspeed", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);
}

void test_observing_conditions_average_period_always_zero(void)
{
    ObservingConditionsSnapshot snap; // dataValid false - shouldn't matter
    PropertyResult r = getObservingConditionsProperty("averageperiod", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value);
}

void test_observing_conditions_no_data_error(void)
{
    ObservingConditionsSnapshot snap;
    snap.dataValid = false;

    PropertyResult r = getObservingConditionsProperty("humidity", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_DRIVER_BASE, r.errorNumber);
}

void test_observing_conditions_unknown_property(void)
{
    ObservingConditionsSnapshot snap;
    snap.dataValid = true;

    PropertyResult r = getObservingConditionsProperty("bogus", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);
}

// --- AlpacaDiscovery ---

void test_discovery_valid_packet(void)
{
    const char *payload = "alpacadiscovery1";
    TEST_ASSERT_TRUE(isValidDiscoveryRequest(reinterpret_cast<const uint8_t *>(payload), strlen(payload)));
}

void test_discovery_rejects_wrong_payload(void)
{
    const char *payload = "not-alpaca-at-all";
    TEST_ASSERT_FALSE(isValidDiscoveryRequest(reinterpret_cast<const uint8_t *>(payload), strlen(payload)));
}

void test_discovery_rejects_short_payload(void)
{
    const char *payload = "alpaca";
    TEST_ASSERT_FALSE(isValidDiscoveryRequest(reinterpret_cast<const uint8_t *>(payload), strlen(payload)));
}

void test_discovery_rejects_null(void)
{
    TEST_ASSERT_FALSE(isValidDiscoveryRequest(nullptr, 0));
}

void test_discovery_response_body(void)
{
    std::string body = buildDiscoveryResponse(80);
    TEST_ASSERT_EQUAL_STRING("{\"AlpacaPort\":80}", body.c_str());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(test_safe_when_all_thresholds_pass);
    RUN_TEST(test_unsafe_before_any_good_data);
    RUN_TEST(test_unsafe_when_data_stale);
    RUN_TEST(test_manual_override_forces_unsafe);
    RUN_TEST(test_required_sensor_fault_forces_unsafe);
    RUN_TEST(test_cloud_cover_threshold);
    RUN_TEST(test_cloud_cover_disabled_ignored);
    RUN_TEST(test_sqm_min_threshold);
    RUN_TEST(test_humidity_max_threshold);
    RUN_TEST(test_dewpoint_margin_threshold);
    RUN_TEST(test_stale_data_suppresses_threshold_checks);

    RUN_TEST(test_observing_conditions_maps_known_property);
    RUN_TEST(test_observing_conditions_case_insensitive);
    RUN_TEST(test_observing_conditions_not_implemented_property);
    RUN_TEST(test_observing_conditions_average_period_always_zero);
    RUN_TEST(test_observing_conditions_no_data_error);
    RUN_TEST(test_observing_conditions_unknown_property);

    RUN_TEST(test_discovery_valid_packet);
    RUN_TEST(test_discovery_rejects_wrong_payload);
    RUN_TEST(test_discovery_rejects_short_payload);
    RUN_TEST(test_discovery_rejects_null);
    RUN_TEST(test_discovery_response_body);

    return UNITY_END();
}
