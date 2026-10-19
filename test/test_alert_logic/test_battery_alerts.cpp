#include <unity.h>

#include "AlertEngine.h"

// Battery alert events (specs/032-battery, contracts/integrations.md).

using namespace SQM::Alerts;

namespace
{
    AlertRules rules()
    {
        AlertRules r;
        r.startupGraceSeconds = 0;
        r.cooldownSeconds = 60;
        r.onChargeComplete = true;
        r.onPowerRestored = true;
        return r;
    }

    AlertInputs at(uint32_t now, float percent, bool valid = true)
    {
        AlertInputs in;
        in.nowSeconds = now;
        in.battery.present = true;
        in.battery.valid = valid;
        in.battery.percent = percent;
        return in;
    }

    int count(const std::vector<Alert> &alerts, AlertType type)
    {
        int n = 0;
        for (const Alert &a : alerts)
            if (a.type == type)
                ++n;
        return n;
    }
} // namespace

void test_battery_low_and_critical_once_per_crossing(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    TEST_ASSERT_EQUAL(0, e.update(at(0, 50.0f), r).size());
    std::vector<Alert> out = e.update(at(10, 19.5f), r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::BatteryLow));
    TEST_ASSERT_EQUAL_STRING("Battery low", out[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("Battery at 20% (low below 20%).", out[0].message.c_str());
    // Hovering just above the threshold doesn't re-arm.
    TEST_ASSERT_EQUAL(0, e.update(at(200, 21.0f), r).size());
    TEST_ASSERT_EQUAL(0, e.update(at(400, 19.0f), r).size());
    out = e.update(at(500, 9.9f), r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::BatteryCritical));
    TEST_ASSERT_EQUAL(0, count(out, AlertType::BatteryLow));

    // Charged back above low + 5: re-armed; the next crossing alerts again.
    e.update(at(1000, 26.0f), r);
    TEST_ASSERT_EQUAL(1, count(e.update(at(2000, 18.0f), r), AlertType::BatteryLow));
}

void test_battery_invalid_data_never_fires(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    e.update(at(0, 50.0f), r);
    TEST_ASSERT_EQUAL(0, e.update(at(100, 3.0f, false), r).size());
    AlertInputs gone = at(200, 3.0f);
    gone.battery.present = false;
    TEST_ASSERT_EQUAL(0, e.update(gone, r).size());
}

void test_charge_complete_and_fault_on_entering(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    e.update(at(0, 95.0f), r);
    AlertInputs in = at(100, 100.0f);
    in.battery.charged = true;
    TEST_ASSERT_EQUAL(1, count(e.update(in, r), AlertType::ChargeComplete));
    in.nowSeconds = 200;
    TEST_ASSERT_EQUAL(0, e.update(in, r).size());

    in.battery.charged = false;
    in.battery.fault = true;
    in.nowSeconds = 300;
    std::vector<Alert> out = e.update(in, r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::ChargeFault));
    TEST_ASSERT_EQUAL_STRING("Charging fault", out.back().title.c_str());
    in.nowSeconds = 400;
    TEST_ASSERT_EQUAL(0, count(e.update(in, r), AlertType::ChargeFault)); // once until it leaves
}

void test_battery_cooldown_applies(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.cooldownSeconds = 600;
    e.update(at(0, 50.0f), r);
    TEST_ASSERT_EQUAL(1, count(e.update(at(10, 19.0f), r), AlertType::BatteryLow));
    // Back up within the cooldown and down again: still low, nothing new.
    e.update(at(20, 30.0f), r);
    TEST_ASSERT_EQUAL(0, count(e.update(at(30, 19.0f), r), AlertType::BatteryLow));
    // Re-armed once the cooldown has passed; the next crossing waits out the cooldown too.
    e.update(at(700, 30.0f), r);
    TEST_ASSERT_EQUAL(0, count(e.update(at(800, 19.0f), r), AlertType::BatteryLow));
    TEST_ASSERT_EQUAL(1, count(e.update(at(1400, 19.0f), r), AlertType::BatteryLow));
}

// SC-003: a full discharge (1 % a minute here) gives exactly one low alert.
void test_full_discharge_gives_one_low_alert(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    int low = 0;
    int critical = 0;
    // A noisy reading: +-1 % around the falling charge.
    for (uint32_t t = 0; t <= 100 * 60; t += 30)
    {
        const float percent = 100.0f - static_cast<float>(t) / 60.0f + ((t / 30) % 2 == 0 ? 1.0f : -1.0f);
        const std::vector<Alert> out = e.update(at(t, percent < 0 ? 0 : percent), r);
        low += count(out, AlertType::BatteryLow);
        critical += count(out, AlertType::BatteryCritical);
    }
    TEST_ASSERT_EQUAL(1, low);
    TEST_ASSERT_EQUAL(1, critical);
}

void test_shutdown_and_power_restored_skip_grace_and_cooldown(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.startupGraceSeconds = 60;
    r.cooldownSeconds = 3600;
    AlertInputs in = at(0, 4.0f);
    in.battery.shutdownStarted = true;
    std::vector<Alert> out = e.update(in, r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::BatteryShutdown));
    TEST_ASSERT_EQUAL_STRING("Going to sleep: battery empty", out[0].title.c_str());
    in = at(5, 4.0f);
    in.battery.powerRestored = true;
    TEST_ASSERT_EQUAL(1, count(e.update(in, r), AlertType::PowerRestored));
    in.nowSeconds = 10;
    TEST_ASSERT_EQUAL(1, count(e.update(in, r), AlertType::PowerRestored)); // one-shot by construction, not by cooldown
    r.onPowerRestored = false;
    TEST_ASSERT_EQUAL(0, count(e.update(in, r), AlertType::PowerRestored));
}

void test_battery_rule_alert_when_its_bits_appear(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.onBatteryRule = true;
    AlertInputs in = at(0, 50.0f);
    in.safetyKnown = true;
    in.isSafe = true;
    e.update(in, r);
    in.nowSeconds = 10;
    in.isSafe = false;
    in.unsafeFlags = 1u << 14;
    in.unsafeReasons = {"Cloud 96% >= 90%", "Battery 8% < 10%"};
    std::vector<Alert> out = e.update(in, r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::BatteryRule));
    for (const Alert &a : out)
        if (a.type == AlertType::BatteryRule)
        {
            TEST_ASSERT_EQUAL_STRING("Battery: unsafe", a.title.c_str());
            TEST_ASSERT_EQUAL_STRING("Battery 8% < 10%", a.message.c_str());
            TEST_ASSERT_EQUAL(static_cast<int>(AlertLevel::Urgent), static_cast<int>(a.level));
        }
    // Still there: nothing new. Data missing appears: again, once the cooldown has passed.
    in.nowSeconds = 20;
    TEST_ASSERT_EQUAL(0, count(e.update(in, r), AlertType::BatteryRule));
    in.nowSeconds = 80;
    in.unsafeFlags = 1u << 15;
    in.unsafeReasons = {"Battery data missing"};
    TEST_ASSERT_EQUAL(1, count(e.update(in, r), AlertType::BatteryRule));
    TEST_ASSERT_EQUAL_STRING("battery_rule", alertTypeName(AlertType::BatteryRule));
}

// The reason flipping between low and missing (data that comes and goes)
// alerts and rings phones at most once per cooldown.
void test_battery_rule_flapping_alerts_once_per_cooldown(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.onBatteryRule = true;
    AlertInputs in = at(0, 8.0f);
    in.safetyKnown = true;
    in.unsafeFlags = 1u << 14;
    in.unsafeReasons = {"Battery 8% < 10%"};
    TEST_ASSERT_EQUAL(1, count(e.update(in, r), AlertType::BatteryRule));
    int sent = 0;
    for (uint32_t t = 1; t < 60; ++t)
    {
        in.nowSeconds = t;
        in.unsafeFlags = (t % 2) != 0 ? (1u << 15) : (1u << 14);
        in.unsafeReasons = {(t % 2) != 0 ? "Battery data missing" : "Battery 8% < 10%"};
        sent += count(e.update(in, r), AlertType::BatteryRule);
    }
    TEST_ASSERT_EQUAL(0, sent);
}

// Switched on with a half-flat pack (the travel case): low and critical are
// announced once the startup grace ends, not taken as the starting state.
void test_battery_already_low_at_power_on_is_announced(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.startupGraceSeconds = 60;
    TEST_ASSERT_EQUAL(0, e.update(at(0, 8.0f), r).size());
    TEST_ASSERT_EQUAL(0, e.update(at(30, 8.0f), r).size());
    std::vector<Alert> out = e.update(at(61, 8.0f), r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::BatteryLow));
    TEST_ASSERT_EQUAL(1, count(out, AlertType::BatteryCritical));
    TEST_ASSERT_EQUAL(0, e.update(at(200, 8.0f), r).size()); // once
    // A healthy pack at power-on: nothing.
    AlertEngine healthy;
    healthy.update(at(0, 80.0f), r);
    TEST_ASSERT_EQUAL(0, healthy.update(at(61, 80.0f), r).size());
}

// The gauge's first percent arrives after its estimating window (after the grace).
void test_battery_low_when_first_valid_reading_arrives(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    TEST_ASSERT_EQUAL(0, e.update(at(0, 0.0f, false), r).size());
    TEST_ASSERT_EQUAL(1, count(e.update(at(40, 15.0f), r), AlertType::BatteryLow));
}

void test_charge_fault_present_at_power_on_is_announced(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.startupGraceSeconds = 60;
    AlertInputs in = at(0, 50.0f);
    in.battery.fault = true;
    TEST_ASSERT_EQUAL(0, e.update(in, r).size());
    in.nowSeconds = 61;
    TEST_ASSERT_EQUAL(1, count(e.update(in, r), AlertType::ChargeFault));
    // Charged at power-on is no news.
    AlertEngine full;
    AlertInputs charged = at(0, 100.0f);
    charged.battery.charged = true;
    full.update(charged, r);
    charged.nowSeconds = 61;
    TEST_ASSERT_EQUAL(0, count(full.update(charged, r), AlertType::ChargeComplete));
}

// Power saving ended because the charge climbed back without USB.
void test_recovered_without_usb_says_so(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    AlertInputs in = at(0, 16.0f);
    in.battery.recovered = true;
    std::vector<Alert> out = e.update(in, r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::PowerRestored));
    TEST_ASSERT_EQUAL_STRING("Battery recovered", out[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("Battery back at 16%: power saving is off (it starts below 10%).", out[0].message.c_str());
}

void run_battery_alert_tests(void)
{
    RUN_TEST(test_battery_rule_flapping_alerts_once_per_cooldown);
    RUN_TEST(test_battery_already_low_at_power_on_is_announced);
    RUN_TEST(test_battery_low_when_first_valid_reading_arrives);
    RUN_TEST(test_charge_fault_present_at_power_on_is_announced);
    RUN_TEST(test_recovered_without_usb_says_so);
    RUN_TEST(test_battery_low_and_critical_once_per_crossing);
    RUN_TEST(test_battery_invalid_data_never_fires);
    RUN_TEST(test_charge_complete_and_fault_on_entering);
    RUN_TEST(test_battery_cooldown_applies);
    RUN_TEST(test_full_discharge_gives_one_low_alert);
    RUN_TEST(test_shutdown_and_power_restored_skip_grace_and_cooldown);
    RUN_TEST(test_battery_rule_alert_when_its_bits_appear);
}
