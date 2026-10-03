#include <unity.h>
#include "AlertEngine.h"

using namespace SQM::Alerts;

void setUp(void) {}
void tearDown(void) {}

namespace
{
    AlertRules noGraceRules()
    {
        AlertRules rules;
        rules.startupGraceSeconds = 0;
        rules.cooldownSeconds = 60;
        return rules;
    }

    AlertInputs safeInputs(uint32_t now)
    {
        AlertInputs in;
        in.nowSeconds = now;
        in.safetyKnown = true;
        in.isSafe = true;
        in.rainEnabled = true;
        for (size_t i = 0; i < SENSOR_COUNT; ++i)
        {
            in.sensors[i].name = "Sensor";
            in.sensors[i].enabled = i < 4;
            in.sensors[i].healthy = true;
        }
        in.environmentValid = true;
        in.temperatureC = 10.0f;
        in.dewpointC = 0.0f;
        in.skyValid = true;
        in.cloudCoverPercent = 80.0f;
        return in;
    }

    bool hasType(const std::vector<Alert> &alerts, AlertType type)
    {
        for (const Alert &a : alerts)
            if (a.type == type)
                return true;
        return false;
    }
}

void test_first_update_sets_baseline_without_alerts(void)
{
    AlertEngine engine;
    AlertInputs in = safeInputs(0);
    in.isSafe = false; // device boots unsafe
    TEST_ASSERT_EQUAL(0, engine.update(in, noGraceRules()).size());
}

void test_unsafe_then_safe(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    engine.update(safeInputs(0), rules);

    AlertInputs in = safeInputs(10);
    in.isSafe = false;
    in.unsafeReasons = {"Cloud cover at or above unsafe threshold"};
    std::vector<Alert> alerts = engine.update(in, rules);
    TEST_ASSERT_EQUAL(1, alerts.size());
    TEST_ASSERT_TRUE(alerts[0].type == AlertType::Unsafe);
    TEST_ASSERT_TRUE(alerts[0].priority == AlertPriority::High);
    TEST_ASSERT_EQUAL_STRING("Cloud cover at or above unsafe threshold", alerts[0].message.c_str());

    // Still unsafe -> nothing new
    TEST_ASSERT_EQUAL(0, engine.update(in, rules).size());

    alerts = engine.update(safeInputs(100), rules);
    TEST_ASSERT_TRUE(hasType(alerts, AlertType::Safe));
}

void test_cooldown_defers_but_does_not_lose_final_state(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules(); // 60 s cooldown
    engine.update(safeInputs(0), rules);

    AlertInputs unsafe = safeInputs(10);
    unsafe.isSafe = false;
    TEST_ASSERT_TRUE(hasType(engine.update(unsafe, rules), AlertType::Unsafe));

    // Flaps back to safe within the cooldown: suppressed for now
    TEST_ASSERT_EQUAL(0, engine.update(safeInputs(20), rules).size());
    // Cooldown over and still safe: the deferred "safe" goes out
    TEST_ASSERT_TRUE(hasType(engine.update(safeInputs(75), rules), AlertType::Safe));
}

void test_flap_within_cooldown_sends_nothing_extra(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    engine.update(safeInputs(0), rules);

    AlertInputs unsafe = safeInputs(10);
    unsafe.isSafe = false;
    engine.update(unsafe, rules);
    engine.update(safeInputs(20), rules); // suppressed
    unsafe.nowSeconds = 30;
    engine.update(unsafe, rules); // back to the notified state
    unsafe.nowSeconds = 90;
    TEST_ASSERT_EQUAL(0, engine.update(unsafe, rules).size());
}

void test_startup_grace(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.startupGraceSeconds = 60;
    engine.update(safeInputs(0), rules);

    AlertInputs unsafe = safeInputs(30);
    unsafe.isSafe = false;
    TEST_ASSERT_EQUAL(0, engine.update(unsafe, rules).size()); // within grace
    unsafe.nowSeconds = 61;
    TEST_ASSERT_EQUAL(0, engine.update(unsafe, rules).size()); // baseline is now "unsafe"
    TEST_ASSERT_TRUE(hasType(engine.update(safeInputs(62), rules), AlertType::Safe));
}

void test_rain_started_and_stopped(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    engine.update(safeInputs(0), rules);

    AlertInputs wet = safeInputs(5);
    wet.raining = true;
    wet.rainRateMmPerHour = 1.2f;
    std::vector<Alert> alerts = engine.update(wet, rules);
    TEST_ASSERT_TRUE(hasType(alerts, AlertType::RainStarted));

    alerts = engine.update(safeInputs(900), rules);
    TEST_ASSERT_TRUE(hasType(alerts, AlertType::RainStopped));
}

void test_disabled_rule_is_silent_and_does_not_fire_on_enable(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onRain = false;
    engine.update(safeInputs(0), rules);

    AlertInputs wet = safeInputs(5);
    wet.raining = true;
    TEST_ASSERT_FALSE(hasType(engine.update(wet, rules), AlertType::RainStarted));

    rules.onRain = true;
    wet.nowSeconds = 6;
    TEST_ASSERT_FALSE(hasType(engine.update(wet, rules), AlertType::RainStarted));
}

void test_sensor_fault_and_recovery(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    engine.update(safeInputs(0), rules);

    AlertInputs faulty = safeInputs(5);
    faulty.sensors[2].name = "BME280";
    faulty.sensors[2].healthy = false;
    std::vector<Alert> alerts = engine.update(faulty, rules);
    TEST_ASSERT_TRUE(hasType(alerts, AlertType::SensorFault));
    TEST_ASSERT_EQUAL_STRING("BME280 sensor fault", alerts[0].title.c_str());

    TEST_ASSERT_TRUE(hasType(engine.update(safeInputs(100), rules), AlertType::SensorRecovered));
}

void test_disabled_sensor_never_alerts(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    AlertInputs in = safeInputs(0);
    in.sensors[4].enabled = false;
    in.sensors[4].healthy = false;
    engine.update(in, rules);
    in.nowSeconds = 10;
    TEST_ASSERT_EQUAL(0, engine.update(in, rules).size());
}

void test_dew_risk_with_hysteresis(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onDewRisk = true;
    rules.dewRiskMarginC = 2.0f;
    rules.cooldownSeconds = 0;
    engine.update(safeInputs(0), rules);

    AlertInputs in = safeInputs(10);
    in.dewpointC = 8.5f; // margin 1.5
    TEST_ASSERT_TRUE(hasType(engine.update(in, rules), AlertType::DewRisk));

    in.nowSeconds = 20;
    in.dewpointC = 7.8f; // margin 2.2 - inside the hysteresis band, still at risk
    TEST_ASSERT_EQUAL(0, engine.update(in, rules).size());

    in.nowSeconds = 30;
    in.dewpointC = 7.0f; // margin 3.0 - cleared (no alert for clearing)
    TEST_ASSERT_EQUAL(0, engine.update(in, rules).size());

    in.nowSeconds = 40;
    in.dewpointC = 8.5f;
    TEST_ASSERT_TRUE(hasType(engine.update(in, rules), AlertType::DewRisk));
}

void test_clear_sky(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onClearSky = true;
    engine.update(safeInputs(0), rules);

    AlertInputs in = safeInputs(10);
    in.cloudCoverPercent = 10.0f;
    TEST_ASSERT_TRUE(hasType(engine.update(in, rules), AlertType::ClearSky));
}

void test_alert_type_names(void)
{
    TEST_ASSERT_EQUAL_STRING("rain_started", alertTypeName(AlertType::RainStarted));
    TEST_ASSERT_EQUAL_STRING("unsafe", alertTypeName(AlertType::Unsafe));
    TEST_ASSERT_EQUAL_STRING("a; b", joinReasons({"a", "b"}).c_str());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_update_sets_baseline_without_alerts);
    RUN_TEST(test_unsafe_then_safe);
    RUN_TEST(test_cooldown_defers_but_does_not_lose_final_state);
    RUN_TEST(test_flap_within_cooldown_sends_nothing_extra);
    RUN_TEST(test_startup_grace);
    RUN_TEST(test_rain_started_and_stopped);
    RUN_TEST(test_disabled_rule_is_silent_and_does_not_fire_on_enable);
    RUN_TEST(test_sensor_fault_and_recovery);
    RUN_TEST(test_disabled_sensor_never_alerts);
    RUN_TEST(test_dew_risk_with_hysteresis);
    RUN_TEST(test_clear_sky);
    RUN_TEST(test_alert_type_names);
    return UNITY_END();
}
