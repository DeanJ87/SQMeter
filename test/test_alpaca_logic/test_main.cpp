#include <unity.h>
#include <cstring>
#include "SafetyEvaluator.h"
#include "ObservingConditionsMapper.h"
#include "AlpacaDiscovery.h"
#include "AlpacaProtocol.h"

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

namespace
{
    ObservingConditionsSnapshot allValidSnapshot()
    {
        ObservingConditionsSnapshot snap;
        for (SourceState *source : {&snap.skyLight, &snap.irSky, &snap.environment})
        {
            source->present = true;
            source->valid = true;
        }
        return snap;
    }
}

void test_observing_conditions_maps_known_property(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.skyQualityMagArcsec2 = 21.3f;

    PropertyResult r = getObservingConditionsProperty("SkyQuality", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(21.3f, r.value);
}

void test_observing_conditions_case_insensitive(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.temperatureC = 12.5f;

    PropertyResult r = getObservingConditionsProperty("TEMPERATURE", snap);
    TEST_ASSERT_TRUE(r.ok);
}

void test_observing_conditions_not_implemented_property(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot(); // wind not present

    PropertyResult r = getObservingConditionsProperty("windspeed", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);

    r = getObservingConditionsProperty("starfwhm", snap);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);
}

void test_observing_conditions_average_period_always_zero(void)
{
    ObservingConditionsSnapshot snap; // nothing valid - shouldn't matter
    PropertyResult r = getObservingConditionsProperty("averageperiod", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value);
}

void test_observing_conditions_no_data_error(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.environment.valid = false;

    PropertyResult r = getObservingConditionsProperty("humidity", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_DRIVER_BASE, r.errorNumber);
}

void test_observing_conditions_one_sensor_down_others_ok(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.environment.valid = false;
    snap.skyTemperatureC = -20.0f;

    TEST_ASSERT_FALSE(getObservingConditionsProperty("pressure", snap).ok);
    PropertyResult r = getObservingConditionsProperty("skytemperature", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(-20.0f, r.value);
}

void test_observing_conditions_unknown_property(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();

    PropertyResult r = getObservingConditionsProperty("bogus", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);
}

void test_observing_conditions_pressure(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.pressureHPa = 1013.2f;

    PropertyResult r = getObservingConditionsProperty("pressure", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(1013.2f, r.value);
}

void test_observing_conditions_rainrate_depends_on_rain_sensor(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.rainRateMmPerHour = 2.5f;

    // Rain sensor disabled -> NotImplemented
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getObservingConditionsProperty("rainrate", snap).errorNumber);

    // Enabled but offline -> driver error
    snap.rain.present = true;
    TEST_ASSERT_EQUAL(ALPACA_ERR_DRIVER_BASE, getObservingConditionsProperty("rainrate", snap).errorNumber);

    snap.rain.valid = true;
    PropertyResult r = getObservingConditionsProperty("rainrate", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, r.value);
}

void test_rain_rate_imperial_conversion(void)
{
    TEST_ASSERT_EQUAL_FLOAT(25.4f, rainRateToMmPerHour(1.0f, true));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, rainRateToMmPerHour(3.0f, false));
}

void test_wind_direction_zero_when_calm(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.wind.present = true;
    snap.wind.valid = true;
    snap.windVane.present = true;
    snap.windVane.valid = true;
    snap.windDirectionDeg = 270.0f;
    snap.windSpeedMs = 0.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, getObservingConditionsProperty("winddirection", snap).value);
    snap.windSpeedMs = 3.0f;
    TEST_ASSERT_EQUAL_FLOAT(270.0f, getObservingConditionsProperty("winddirection", snap).value);
}

void test_sensor_description(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();

    StringResult r = getSensorDescription("Pressure", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_TRUE(r.value.find("BME280") != std::string::npos);

    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getSensorDescription("StarFWHM", snap).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getSensorDescription("RainRate", snap).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, getSensorDescription("bogus", snap).errorNumber);
}

void test_time_since_last_update(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.skyLight.ageSeconds = 0.4;
    snap.irSky.ageSeconds = 3.0;
    snap.environment.ageSeconds = 2.0;

    PropertyResult r = getTimeSinceLastUpdate("", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(0.4, r.value);

    r = getTimeSinceLastUpdate("humidity", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(2.0, r.value);

    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getTimeSinceLastUpdate("windspeed", snap).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, getTimeSinceLastUpdate("bogus", snap).errorNumber);

    ObservingConditionsSnapshot empty;
    TEST_ASSERT_EQUAL(ALPACA_ERR_DRIVER_BASE, getTimeSinceLastUpdate("", empty).errorNumber);
}

void test_average_period_validation(void)
{
    TEST_ASSERT_TRUE(validateAveragePeriod(0.0).ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, validateAveragePeriod(1.0).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, validateAveragePeriod(-1.0).errorNumber);
}

void test_alpaca_double_parsing(void)
{
    double v = -1.0;
    TEST_ASSERT_TRUE(parseAlpacaDouble("0", v));
    TEST_ASSERT_EQUAL_FLOAT(0.0, v);
    TEST_ASSERT_TRUE(parseAlpacaDouble("1.5", v));
    TEST_ASSERT_EQUAL_FLOAT(1.5, v);
    TEST_ASSERT_FALSE(parseAlpacaDouble("", v));
    TEST_ASSERT_FALSE(parseAlpacaDouble("1.5x", v));
    TEST_ASSERT_FALSE(parseAlpacaDouble("nan", v));
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

// --- AlpacaProtocol ---

void test_param_name_case_insensitive(void)
{
    TEST_ASSERT_TRUE(paramNameEquals("ClientTransactionID", "clienttransactionid"));
    TEST_ASSERT_TRUE(paramNameEquals("CONNECTED", "Connected"));
    TEST_ASSERT_FALSE(paramNameEquals("ClientID", "ClientTransactionID"));
    TEST_ASSERT_FALSE(paramNameEquals("Connected", "Connecte"));
}

void test_client_transaction_id_parsing(void)
{
    TEST_ASSERT_EQUAL_UINT32(42, parseClientTransactionId("42"));
    TEST_ASSERT_EQUAL_UINT32(4294967295u, parseClientTransactionId("4294967295"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId("4294967296"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId("-1"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId("abc"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId(""));
}

void test_alpaca_bool_parsing(void)
{
    bool value = false;
    TEST_ASSERT_TRUE(parseAlpacaBool("True", value));
    TEST_ASSERT_TRUE(value);
    TEST_ASSERT_TRUE(parseAlpacaBool("false", value));
    TEST_ASSERT_FALSE(value);
    TEST_ASSERT_FALSE(parseAlpacaBool("1", value));
    TEST_ASSERT_FALSE(parseAlpacaBool("", value));
}

void test_unique_id_includes_mac(void)
{
    TEST_ASSERT_EQUAL_STRING("sqmeter-a1b2c3d4e5f6-observingconditions-0",
                             buildUniqueId(0xA1B2C3D4E5F6ULL, "observingconditions", 0).c_str());
    // Upper 16 bits of the efuse value are ignored; short MACs are zero-padded.
    TEST_ASSERT_EQUAL_STRING("sqmeter-000000000001-safetymonitor-0",
                             buildUniqueId(0xFFFF000000000001ULL, "safetymonitor", 0).c_str());
}

// --- Rain safety ---

namespace
{
    SafetyInputs freshSafeInputs()
    {
        SafetyInputs in;
        in.hasEverHadGoodData = true;
        in.secondsSinceLastGoodData = 1;
        in.cloudCoverPercent = 5.0f;
        in.rainSensorEnabled = true;
        in.rainSensorHealthy = true;
        return in;
    }
}

void test_rain_makes_unsafe(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);

    in.raining = true;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UNSAFE_RAIN, r.reasonFlags);
}

void test_rain_checked_even_when_other_data_stale(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    in.secondsSinceLastGoodData = 9999;
    in.raining = true;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_TRUE(r.reasonFlags & UNSAFE_RAIN);
    TEST_ASSERT_TRUE(r.reasonFlags & UNSAFE_STALE_DATA);
}

void test_rain_rule_disabled_or_sensor_absent(void)
{
    SafetyThresholds t;
    t.rainUnsafeEnabled = false;
    SafetyInputs in = freshSafeInputs();
    in.raining = true;
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);

    t.rainUnsafeEnabled = true;
    in.rainSensorEnabled = false; // no RG-15 fitted
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);
}

void test_rain_sensor_required(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    in.rainSensorHealthy = false;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UNSAFE_RAIN_SENSOR_FAULT, r.reasonFlags);

    t.rainSensorRequired = false;
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);
}

void test_environment_fault_blocks_humidity_rules(void)
{
    SafetyThresholds t;
    SafetyInputs in = freshSafeInputs();
    in.environmentSensorFault = true;
    in.humidityPercent = 53.0f; // fallback value must not be trusted
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe); // rules disabled -> irrelevant

    t.humidityMaxEnabled = true;
    t.humidityMaxSafe = 90.0f;
    SafetyResult r = evaluateSafety(in, t);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UNSAFE_ENVIRONMENT_FAULT, r.reasonFlags);
}

void test_faulted_sensor_skips_its_threshold(void)
{
    SafetyThresholds t;
    t.sqmMinEnabled = true;
    t.sqmMinSafe = 18.0f;
    SafetyInputs in = freshSafeInputs();
    in.requiredSensorFault = true;
    in.irSkyFault = true;
    in.skyLightFault = true;
    in.cloudCoverPercent = 100.0f; // computed from zeroed MLX readings
    in.sqm = 0.0f;                 // from zeroed TSL readings
    TEST_ASSERT_EQUAL_UINT32(UNSAFE_SENSOR_FAULT, evaluateSafety(in, t).reasonFlags);
}

void test_safe_delay_filter(void)
{
    SafeDelayFilter f;
    TEST_ASSERT_FALSE(f.update(true, 100, 60)); // timer starts at first safe
    TEST_ASSERT_EQUAL_UINT32(60, f.secondsUntilSafe());
    TEST_ASSERT_FALSE(f.update(true, 159, 60));
    TEST_ASSERT_TRUE(f.update(true, 160, 60));
    TEST_ASSERT_FALSE(f.update(false, 161, 60)); // unsafe is immediate
    TEST_ASSERT_FALSE(f.update(true, 162, 60));  // and restarts the delay
    TEST_ASSERT_TRUE(f.update(true, 222, 60));

    SafeDelayFilter immediate;
    TEST_ASSERT_TRUE(immediate.update(true, 5, 0));
}

// --- Wind safety ---

void test_wind_limits(void)
{
    SafetyThresholds t;
    t.windSpeedUnsafeEnabled = true;
    t.windSpeedUnsafeMs = 10.0f;
    t.windGustUnsafeEnabled = true;
    t.windGustUnsafeMs = 15.0f;
    SafetyInputs in = freshSafeInputs();
    in.windSensorEnabled = true;
    in.windSensorHealthy = true;
    in.windSpeedMs = 5.0f;
    in.windGustMs = 9.0f;
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);

    in.windSpeedMs = 10.0f;
    TEST_ASSERT_EQUAL_UINT32(UNSAFE_WIND, evaluateSafety(in, t).reasonFlags);

    in.windSpeedMs = 5.0f;
    in.windGustMs = 16.0f;
    TEST_ASSERT_EQUAL_UINT32(UNSAFE_WIND_GUST, evaluateSafety(in, t).reasonFlags);
}

void test_wind_limit_without_sensor_is_unsafe(void)
{
    SafetyThresholds t;
    t.windGustUnsafeEnabled = true;
    SafetyInputs in = freshSafeInputs();
    TEST_ASSERT_EQUAL_UINT32(UNSAFE_WIND_SENSOR_FAULT, evaluateSafety(in, t).reasonFlags);

    t.windGustUnsafeEnabled = false; // no wind rules -> anemometer irrelevant
    TEST_ASSERT_TRUE(evaluateSafety(in, t).isSafe);
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
    RUN_TEST(test_rain_makes_unsafe);
    RUN_TEST(test_rain_checked_even_when_other_data_stale);
    RUN_TEST(test_rain_rule_disabled_or_sensor_absent);
    RUN_TEST(test_rain_sensor_required);
    RUN_TEST(test_environment_fault_blocks_humidity_rules);
    RUN_TEST(test_faulted_sensor_skips_its_threshold);
    RUN_TEST(test_safe_delay_filter);
    RUN_TEST(test_wind_limits);
    RUN_TEST(test_wind_limit_without_sensor_is_unsafe);

    RUN_TEST(test_observing_conditions_maps_known_property);
    RUN_TEST(test_observing_conditions_case_insensitive);
    RUN_TEST(test_observing_conditions_not_implemented_property);
    RUN_TEST(test_observing_conditions_average_period_always_zero);
    RUN_TEST(test_observing_conditions_no_data_error);
    RUN_TEST(test_observing_conditions_unknown_property);
    RUN_TEST(test_observing_conditions_one_sensor_down_others_ok);
    RUN_TEST(test_observing_conditions_pressure);
    RUN_TEST(test_observing_conditions_rainrate_depends_on_rain_sensor);
    RUN_TEST(test_rain_rate_imperial_conversion);
    RUN_TEST(test_wind_direction_zero_when_calm);
    RUN_TEST(test_sensor_description);
    RUN_TEST(test_time_since_last_update);
    RUN_TEST(test_average_period_validation);
    RUN_TEST(test_alpaca_double_parsing);

    RUN_TEST(test_discovery_valid_packet);
    RUN_TEST(test_discovery_rejects_wrong_payload);
    RUN_TEST(test_discovery_rejects_short_payload);
    RUN_TEST(test_discovery_rejects_null);
    RUN_TEST(test_discovery_response_body);

    RUN_TEST(test_param_name_case_insensitive);
    RUN_TEST(test_client_transaction_id_parsing);
    RUN_TEST(test_alpaca_bool_parsing);
    RUN_TEST(test_unique_id_includes_mac);

    return UNITY_END();
}
