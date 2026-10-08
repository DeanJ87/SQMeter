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
        rules.sensorSettleSeconds = 0;
        rules.skySettleSeconds = 0;
        rules.skyNightOnly = false;
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
} // namespace

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
    in.unsafeReasons = {"Cloud 62% >= 35%"};
    std::vector<Alert> alerts = engine.update(in, rules);
    TEST_ASSERT_EQUAL(1, alerts.size());
    TEST_ASSERT_TRUE(alerts[0].type == AlertType::Unsafe);
    TEST_ASSERT_EQUAL_STRING("\u2022 Cloud 62% >= 35%", alerts[0].message.c_str());

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

AlertInputs skyAt(uint32_t now, float cloud, bool night = true)
{
    AlertInputs in = safeInputs(now);
    in.cloudCoverPercent = cloud;
    in.nightKnown = true;
    in.isNight = night;
    return in;
}

void test_sky_clears_and_clouds_over(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onClearSky = true;
    rules.onCloudedOver = true;
    rules.cooldownSeconds = 0;
    engine.update(skyAt(0, 80), rules); // baseline: cloudy

    TEST_ASSERT_TRUE(hasType(engine.update(skyAt(10, 10), rules), AlertType::ClearSky));
    // Between the thresholds (20..70): nothing changes
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(20, 50), rules).size());
    TEST_ASSERT_TRUE(hasType(engine.update(skyAt(30, 85), rules), AlertType::CloudedOver));
}

void test_sky_only_enabled_direction(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onClearSky = true; // clouding over not wanted
    rules.cooldownSeconds = 0;
    engine.update(skyAt(0, 80), rules);
    TEST_ASSERT_TRUE(hasType(engine.update(skyAt(10, 10), rules), AlertType::ClearSky));
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(20, 90), rules).size());
    TEST_ASSERT_TRUE(hasType(engine.update(skyAt(30, 5), rules), AlertType::ClearSky));
}

void test_sky_settle(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onClearSky = true;
    rules.skySettleSeconds = 120;
    engine.update(skyAt(0, 80), rules);
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(10, 10), rules).size());
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(60, 80), rules).size()); // a gap closes again
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(100, 10), rules).size());
    TEST_ASSERT_TRUE(hasType(engine.update(skyAt(220, 10), rules), AlertType::ClearSky));
}

void test_sky_night_only(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onClearSky = true;
    rules.onCloudedOver = true;
    rules.skyNightOnly = true;
    rules.cooldownSeconds = 0;
    engine.update(skyAt(0, 80, false), rules);

    // Clears up in the afternoon: held
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(10, 5, false), rules).size());
    // Still clear when it gets dark: announced once, as "Dark and clear"
    std::vector<Alert> alerts = engine.update(skyAt(20, 5, true), rules);
    TEST_ASSERT_TRUE(hasType(alerts, AlertType::ClearSky));
    TEST_ASSERT_EQUAL_STRING("Dark and clear", alerts[0].title.c_str());
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(30, 5, true), rules).size());
    // Clouds over at night: announced
    TEST_ASSERT_TRUE(hasType(engine.update(skyAt(40, 90, true), rules), AlertType::CloudedOver));
    // Dawn: nothing
    TEST_ASSERT_EQUAL(0, engine.update(skyAt(50, 5, false), rules).size());
}

void test_night_only_without_location_does_not_block(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onClearSky = true;
    rules.skyNightOnly = true;
    engine.update(skyAt(0, 80), rules);
    AlertInputs unknown = skyAt(10, 5, false);
    unknown.nightKnown = false;
    TEST_ASSERT_TRUE(hasType(engine.update(unknown, rules), AlertType::ClearSky));
}

void test_alert_type_names(void)
{
    TEST_ASSERT_EQUAL_STRING("rain_started", alertTypeName(AlertType::RainStarted));
    TEST_ASSERT_EQUAL_STRING("unsafe", alertTypeName(AlertType::Unsafe));
    TEST_ASSERT_EQUAL_STRING("clouded_over", alertTypeName(AlertType::CloudedOver));
    TEST_ASSERT_EQUAL_STRING("\u2022 a\n\u2022 b", joinReasons({"a", "b"}).c_str());
}

void test_sensor_blip_is_not_announced(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.sensorSettleSeconds = 30;
    engine.update(safeInputs(0), rules);

    AlertInputs blip = safeInputs(10);
    blip.sensors[1].healthy = false; // e.g. saving settings reconfigures it
    TEST_ASSERT_EQUAL(0, engine.update(blip, rules).size());
    blip.nowSeconds = 15;
    TEST_ASSERT_EQUAL(0, engine.update(blip, rules).size());
    TEST_ASSERT_EQUAL(0, engine.update(safeInputs(16), rules).size()); // recovered: nothing sent

    AlertInputs real = safeInputs(100);
    real.sensors[1].healthy = false;
    TEST_ASSERT_EQUAL(0, engine.update(real, rules).size());
    real.nowSeconds = 129;
    TEST_ASSERT_EQUAL(0, engine.update(real, rules).size());
    real.nowSeconds = 130;
    TEST_ASSERT_TRUE(hasType(engine.update(real, rules), AlertType::SensorFault));
}

void test_render_template(void)
{
    const std::vector<std::pair<std::string, std::string>> vars = {{"sqm", "18.21"}, {"device", "Roof"}};
    TEST_ASSERT_EQUAL_STRING("Roof: SQM 18.21", renderTemplate("{device}: SQM {sqm}", vars).c_str());
    // Unknown names and stray braces stay visible.
    TEST_ASSERT_EQUAL_STRING("{nope} { x", renderTemplate("{nope} { x", vars).c_str());
    TEST_ASSERT_EQUAL_STRING("", renderTemplate("", vars).c_str());
}

void test_unsafe_alert_lists_every_reason(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    engine.update(safeInputs(0), rules);
    AlertInputs in = safeInputs(10);
    in.isSafe = false;
    in.unsafeReasons = {"SQM 18.21 < 19.50", "Cloud 62% >= 35%", "Humidity 92% > 90%"};
    const std::vector<Alert> alerts = engine.update(in, rules);
    TEST_ASSERT_EQUAL(1, alerts.size());
    TEST_ASSERT_EQUAL_STRING("\u2022 SQM 18.21 < 19.50\n\u2022 Cloud 62% >= 35%\n\u2022 Humidity 92% > 90%", alerts[0].message.c_str());
    TEST_ASSERT_EQUAL_STRING("3", renderTemplate("{reason_count}", alerts[0].vars).c_str());
    TEST_ASSERT_EQUAL_STRING(
        "SQM 18.21 < 19.50; Cloud 62% >= 35%; Humidity 92% > 90%", renderTemplate("{reasons_inline}", alerts[0].vars).c_str());
}

void test_stack_alerts(void)
{
    Alert unsafe;
    unsafe.type = AlertType::Unsafe;
    unsafe.level = AlertLevel::Urgent;
    unsafe.title = "Observatory UNSAFE";
    unsafe.message = "reasons";
    Alert rain;
    rain.type = AlertType::RainStarted;
    rain.level = AlertLevel::Wake;
    rain.sound = "siren";
    rain.title = "Rain detected";
    rain.message = "rain";

    const Alert one = stackAlerts({unsafe});
    TEST_ASSERT_EQUAL_STRING("Observatory UNSAFE", one.title.c_str());
    TEST_ASSERT_EQUAL(0, one.stacked.size());

    const Alert both = stackAlerts({unsafe, rain});
    TEST_ASSERT_EQUAL(static_cast<int>(AlertType::RainStarted), static_cast<int>(both.type));
    TEST_ASSERT_EQUAL(static_cast<int>(AlertLevel::Wake), static_cast<int>(both.level));
    TEST_ASSERT_EQUAL_STRING("siren", both.sound.c_str());
    TEST_ASSERT_EQUAL_STRING("Rain detected \u00b7 Observatory UNSAFE", both.title.c_str());
    TEST_ASSERT_EQUAL_STRING("rain\n\nreasons", both.message.c_str());
    TEST_ASSERT_EQUAL(1, both.stacked.size());
    TEST_ASSERT_EQUAL(static_cast<int>(AlertType::Unsafe), static_cast<int>(both.stacked[0]));
}

void test_safe_delay_hold_after_restart_is_not_news(void)
{
    // Boot: the safe delay reports unsafe with nothing failing, then safe.
    // Nobody was told "unsafe", so "safe" mustn't be sent either.
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    AlertInputs held = safeInputs(0);
    held.isSafe = false;
    held.safetySettling = true;
    TEST_ASSERT_EQUAL(0, engine.update(held, rules).size());
    held.nowSeconds = 100;
    TEST_ASSERT_EQUAL(0, engine.update(held, rules).size());
    TEST_ASSERT_EQUAL(0, engine.update(safeInputs(180), rules).size());

    // Mid-run: a real unsafe spell is announced, the hold that follows
    // it is not, and safe is announced once the delay is over.
    AlertInputs unsafe = safeInputs(400);
    unsafe.isSafe = false;
    unsafe.unsafeReasons = {"Cloud 62% >= 35%"};
    TEST_ASSERT_TRUE(hasType(engine.update(unsafe, rules), AlertType::Unsafe));
    AlertInputs tail = safeInputs(800);
    tail.isSafe = false;
    tail.safetySettling = true;
    TEST_ASSERT_EQUAL(0, engine.update(tail, rules).size());
    TEST_ASSERT_TRUE(hasType(engine.update(safeInputs(990), rules), AlertType::Safe));
}

void test_restart_compares_with_what_was_last_sent(void)
{
    AlertRules rules = noGraceRules();
    rules.startupGraceSeconds = 60;

    // Told "unsafe" before the restart; booting unsafe changes nothing.
    {
        AlertEngine engine;
        engine.seedSafety(true);
        AlertInputs noData = safeInputs(1);
        noData.isSafe = false;
        noData.safetySettling = true;
        TEST_ASSERT_EQUAL(0, engine.update(noData, rules).size());
        AlertInputs cloudy = safeInputs(10);
        cloudy.isSafe = false;
        cloudy.unsafeReasons = {"Cloud 62% >= 35%"};
        TEST_ASSERT_EQUAL(0, engine.update(cloudy, rules).size());
        cloudy.nowSeconds = 70;
        TEST_ASSERT_EQUAL(0, engine.update(cloudy, rules).size());
        // ...and when it clears, "safe" is real news.
        TEST_ASSERT_TRUE(hasType(engine.update(safeInputs(400), rules), AlertType::Safe));
    }

    // Told "safe" before the restart; it's unsafe now -> announced once the grace ends.
    {
        AlertEngine engine;
        engine.seedSafety(false);
        AlertInputs cloudy = safeInputs(10);
        cloudy.isSafe = false;
        cloudy.unsafeReasons = {"Cloud 62% >= 35%"};
        TEST_ASSERT_EQUAL(0, engine.update(cloudy, rules).size());
        cloudy.nowSeconds = 71; // grace counts from the first update
        TEST_ASSERT_TRUE(hasType(engine.update(cloudy, rules), AlertType::Unsafe));
    }
}

void test_safety_alerts_only_when_dark(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    AlertInputs night = safeInputs(0);
    night.nightKnown = true;
    night.isNight = true;
    engine.update(night, rules);

    // Dawn: the brightening sky fails the SQM rule - not announced.
    AlertInputs dawn = night;
    dawn.nowSeconds = 1000;
    dawn.isNight = false;
    dawn.isSafe = false;
    dawn.unsafeReasons = {"SQM 17.24 < 17.25"};
    TEST_ASSERT_EQUAL(0, engine.update(dawn, rules).size());
    dawn.nowSeconds = 30000;
    TEST_ASSERT_EQUAL(0, engine.update(dawn, rules).size());

    // Nightfall, cloudy: unsafe compared with the "safe" last announced.
    AlertInputs dusk = dawn;
    dusk.nowSeconds = 60000;
    dusk.isNight = true;
    dusk.unsafeReasons = {"Cloud 62% >= 35%"};
    TEST_ASSERT_TRUE(hasType(engine.update(dusk, rules), AlertType::Unsafe));

    // Turned off: daytime changes are announced.
    AlertEngine always;
    rules.safetyNightOnly = false;
    always.update(night, rules);
    dawn.nowSeconds = 1000;
    TEST_ASSERT_TRUE(hasType(always.update(dawn, rules), AlertType::Unsafe));
}

void test_level_priorities()
{
    TEST_ASSERT_EQUAL(-1, pushoverPriority(AlertLevel::Quiet));
    TEST_ASSERT_EQUAL(0, pushoverPriority(AlertLevel::Normal));
    TEST_ASSERT_EQUAL(1, pushoverPriority(AlertLevel::Urgent));
    TEST_ASSERT_EQUAL(2, pushoverPriority(AlertLevel::Wake));
    TEST_ASSERT_EQUAL_STRING("low", ntfyPriority(AlertLevel::Quiet));
    TEST_ASSERT_EQUAL_STRING("default", ntfyPriority(AlertLevel::Normal));
    TEST_ASSERT_EQUAL_STRING("high", ntfyPriority(AlertLevel::Urgent));
    TEST_ASSERT_EQUAL_STRING("max", ntfyPriority(AlertLevel::Wake));
}

void test_waits_startup_grace(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.startupGraceSeconds = 60;
    TEST_ASSERT_EQUAL(0, engine.waits(0, rules).size()); // not started
    engine.update(safeInputs(0), rules);
    std::vector<Wait> waits = engine.waits(20, rules);
    TEST_ASSERT_EQUAL(1, waits.size());
    TEST_ASSERT_EQUAL_STRING("startup", waits[0].condition.c_str());
    TEST_ASSERT_TRUE(waits[0].kind == WaitKind::Grace);
    TEST_ASSERT_EQUAL(40, waits[0].remainingSeconds);
    TEST_ASSERT_EQUAL(0, engine.waits(60, rules).size());
}

void test_waits_cooldown_and_settle(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.cooldownSeconds = 60;
    rules.sensorSettleSeconds = 30;
    engine.update(safeInputs(0), rules);
    TEST_ASSERT_EQUAL(0, engine.waits(0, rules).size()); // nothing pending

    AlertInputs unsafe = safeInputs(10);
    unsafe.isSafe = false;
    TEST_ASSERT_TRUE(hasType(engine.update(unsafe, rules), AlertType::Unsafe));
    TEST_ASSERT_EQUAL(0, engine.waits(10, rules).size()); // announced, nothing waiting

    // Safe again within the cooldown: held back, 60 - (25 - 10) = 45 s left.
    engine.update(safeInputs(25), rules);
    std::vector<Wait> waits = engine.waits(25, rules);
    TEST_ASSERT_EQUAL(1, waits.size());
    TEST_ASSERT_EQUAL_STRING("safety", waits[0].condition.c_str());
    TEST_ASSERT_TRUE(waits[0].kind == WaitKind::Cooldown);
    TEST_ASSERT_EQUAL(45, waits[0].remainingSeconds);

    // A sensor fault must hold 30 s first.
    AlertInputs fault = safeInputs(26);
    fault.sensors[1].name = "MLX90614";
    fault.sensors[1].healthy = false;
    engine.update(fault, rules);
    waits = engine.waits(36, rules);
    bool found = false;
    for (const Wait &w : waits)
        if (w.condition == "sensor:MLX90614")
        {
            found = true;
            TEST_ASSERT_TRUE(w.kind == WaitKind::Settle);
            TEST_ASSERT_EQUAL(20, w.remainingSeconds);
        }
    TEST_ASSERT_TRUE(found);
}

void test_waits_does_not_change_what_is_sent(void)
{
    AlertRules rules = noGraceRules();
    AlertEngine a;
    AlertEngine b;
    AlertInputs unsafe = safeInputs(5);
    unsafe.isSafe = false;
    a.update(safeInputs(0), rules);
    b.update(safeInputs(0), rules);
    (void)a.waits(3, rules);
    TEST_ASSERT_EQUAL(a.update(unsafe, rules).size(), b.update(unsafe, rules).size());
    (void)a.waits(8, rules);
    TEST_ASSERT_EQUAL(a.update(safeInputs(70), rules).size(), b.update(safeInputs(70), rules).size());
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
    RUN_TEST(test_sky_clears_and_clouds_over);
    RUN_TEST(test_sky_only_enabled_direction);
    RUN_TEST(test_sky_settle);
    RUN_TEST(test_sky_night_only);
    RUN_TEST(test_night_only_without_location_does_not_block);
    RUN_TEST(test_alert_type_names);
    RUN_TEST(test_sensor_blip_is_not_announced);
    RUN_TEST(test_render_template);
    RUN_TEST(test_unsafe_alert_lists_every_reason);
    RUN_TEST(test_stack_alerts);
    RUN_TEST(test_safe_delay_hold_after_restart_is_not_news);
    RUN_TEST(test_restart_compares_with_what_was_last_sent);
    RUN_TEST(test_safety_alerts_only_when_dark);
    RUN_TEST(test_level_priorities);
    RUN_TEST(test_waits_startup_grace);
    RUN_TEST(test_waits_cooldown_and_settle);
    RUN_TEST(test_waits_does_not_change_what_is_sent);
    return UNITY_END();
}
