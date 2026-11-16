#include <unity.h>

#include "AlertEngine.h"

// The sensor fault delay (alerts.sensorFaultDelaySeconds) and the rain-started
// wording on the first drops.

using namespace SQM::Alerts;

namespace
{
    constexpr size_t BME = 2;

    AlertRules rules()
    {
        AlertRules r; // sensorFaultDelaySeconds keeps its default, 120 s
        r.startupGraceSeconds = 0;
        r.cooldownSeconds = 60;
        return r;
    }

    AlertInputs at(uint32_t now, bool bmeHealthy = true)
    {
        AlertInputs in;
        in.nowSeconds = now;
        for (size_t i = 0; i < SENSOR_COUNT; ++i)
        {
            in.sensors[i].name = i == BME ? "BME280" : "Sensor";
            in.sensors[i].enabled = i < 4;
            in.sensors[i].healthy = true;
        }
        in.sensors[BME].healthy = bmeHealthy;
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

    // A stretch of time [from, to) with the BME280 healthy or not.
    struct Span
    {
        uint32_t from;
        uint32_t to;
        bool bmeHealthy;
    };

    // Runs the engine every 10 s over the span and counts what it sends of `type`.
    int run(AlertEngine &e, const AlertRules &r, const Span &span, AlertType type)
    {
        int n = 0;
        for (uint32_t t = span.from; t < span.to; t += 10)
            n += count(e.update(at(t, span.bmeHealthy), r), type);
        return n;
    }

    std::string var(const Alert &alert, const char *name)
    {
        for (const auto &v : alert.vars)
            if (v.first == name)
                return v.second;
        return "<none>";
    }
} // namespace

void test_sensor_fault_shorter_than_the_delay_sends_nothing(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    TEST_ASSERT_EQUAL_UINT32(120, r.sensorFaultDelaySeconds);
    e.update(at(0), r);
    TEST_ASSERT_EQUAL(0, run(e, r, {10, 130, false}, AlertType::SensorFault)); // 110 s of fault
    TEST_ASSERT_EQUAL(0, run(e, r, {130, 600, true}, AlertType::SensorRecovered));
}

void test_sensor_fault_longer_than_the_delay_sends_one_fault_then_one_recovery(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    e.update(at(0), r);
    TEST_ASSERT_EQUAL(0, run(e, r, {10, 130, false}, AlertType::SensorFault));
    std::vector<Alert> out = e.update(at(130, false), r); // held 120 s
    TEST_ASSERT_EQUAL(1, count(out, AlertType::SensorFault));
    TEST_ASSERT_EQUAL_STRING("Sensor fault: BME280", out[0].title.c_str());
    TEST_ASSERT_EQUAL(0, run(e, r, {140, 300, false}, AlertType::SensorFault));

    // The recovery must hold the same 120 s, and is sent once.
    TEST_ASSERT_EQUAL(0, run(e, r, {300, 420, true}, AlertType::SensorRecovered));
    out = e.update(at(420), r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::SensorRecovered));
    TEST_ASSERT_EQUAL_STRING("Sensor recovered: BME280", out[0].title.c_str());
    TEST_ASSERT_EQUAL(0, run(e, r, {430, 900, true}, AlertType::SensorRecovered));
}

void test_sensor_fault_delay_zero_alerts_at_once_and_recovery_waits_the_cooldown(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.sensorFaultDelaySeconds = 0;
    e.update(at(0), r);
    TEST_ASSERT_EQUAL(1, count(e.update(at(10, false), r), AlertType::SensorFault));
    // Back 10 s later: the 60 s cooldown holds the recovery, which still arrives once.
    TEST_ASSERT_EQUAL(0, run(e, r, {20, 70, true}, AlertType::SensorRecovered));
    TEST_ASSERT_EQUAL(1, count(e.update(at(70), r), AlertType::SensorRecovered));
    TEST_ASSERT_EQUAL(0, run(e, r, {80, 300, true}, AlertType::SensorRecovered));
}

void test_sensor_fault_from_boot_is_held_by_the_startup_grace(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.startupGraceSeconds = 60;
    TEST_ASSERT_EQUAL(0, run(e, r, {0, 400, false}, AlertType::SensorFault));
    // Its recovery answers nothing that was sent, so it isn't sent either.
    TEST_ASSERT_EQUAL(0, run(e, r, {400, 800, true}, AlertType::SensorRecovered));
    // A later fault is announced as usual.
    TEST_ASSERT_EQUAL(1, run(e, r, {800, 1000, false}, AlertType::SensorFault));
}

void test_sensor_recovery_only_follows_an_announced_fault(void)
{
    AlertEngine e;
    AlertRules r = rules();
    r.onSensorFault = false;
    e.update(at(0), r);
    TEST_ASSERT_EQUAL(0, run(e, r, {10, 200, false}, AlertType::SensorFault)); // rule off
    r.onSensorFault = true;
    TEST_ASSERT_EQUAL(0, run(e, r, {200, 300, false}, AlertType::SensorFault));
    TEST_ASSERT_EQUAL(0, run(e, r, {300, 800, true}, AlertType::SensorRecovered));
}

void test_lens_fault_waits_the_sensor_fault_delay(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    AlertInputs in = at(0);
    in.rainEnabled = true;
    e.update(in, r);
    in.lensFault = true;
    int sent = 0;
    for (uint32_t t = 10; t < 130; t += 10)
    {
        in.nowSeconds = t;
        sent += count(e.update(in, r), AlertType::LensFault);
    }
    TEST_ASSERT_EQUAL(0, sent);
    in.nowSeconds = 130;
    TEST_ASSERT_EQUAL(1, count(e.update(in, r), AlertType::LensFault));
}

void test_rain_started_on_the_first_drops_has_no_rate_and_is_not_delayed(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    AlertInputs in = at(0);
    in.rainEnabled = true;
    e.update(in, r);
    in.nowSeconds = 5;
    in.raining = true; // latched on accumulation; the RG-15 has no rate yet
    in.rainRateMmPerHour = 0.0f;
    std::vector<Alert> out = e.update(in, r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::RainStarted));
    TEST_ASSERT_EQUAL_STRING("The rain sensor reports the first drops of rain.", out[0].message.c_str());
    TEST_ASSERT_EQUAL_STRING("0.00", var(out[0], "rain_rate").c_str());
}

void test_rain_started_with_a_rate_reports_it(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    AlertInputs in = at(0);
    in.rainEnabled = true;
    e.update(in, r);
    in.nowSeconds = 5;
    in.raining = true;
    in.rainRateMmPerHour = 0.01f; // the RG-15's smallest step
    std::vector<Alert> out = e.update(in, r);
    TEST_ASSERT_EQUAL(1, count(out, AlertType::RainStarted));
    TEST_ASSERT_EQUAL_STRING("The rain sensor reports rain (0.01 mm/h).", out[0].message.c_str());
    TEST_ASSERT_EQUAL_STRING("0.01", var(out[0], "rain_rate").c_str());
}

namespace
{
    // The light, IR and environment sensors with the given health.
    AlertInputs three(uint32_t now, bool lightOk, bool irOk, bool envOk)
    {
        AlertInputs in = at(now);
        in.sensors[0] = {"Light sensor", true, lightOk};
        in.sensors[1] = {"IR sky sensor", true, irOk};
        in.sensors[2] = {"Environment sensor", true, envOk};
        return in;
    }
} // namespace

void test_several_sensors_faulting_together_send_one_grouped_alert(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    e.update(three(0, true, true, true), r);
    for (uint32_t t = 10; t < 130; t += 10)
        TEST_ASSERT_EQUAL(0, e.update(three(t, false, false, false), r).size());
    std::vector<Alert> out = e.update(three(130, false, false, false), r);
    TEST_ASSERT_EQUAL(1, out.size());
    TEST_ASSERT_TRUE(out[0].type == AlertType::SensorFault);
    TEST_ASSERT_EQUAL_STRING("Sensor fault: several sensors", out[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING(
        "Light sensor, IR sky sensor, Environment sensor are offline or reporting errors. Several at once usually means a power or I2C "
        "bus problem.",
        out[0].message.c_str());
    TEST_ASSERT_EQUAL_STRING("Light sensor, IR sky sensor, Environment sensor", var(out[0], "sensor").c_str());

    for (uint32_t t = 300; t < 420; t += 10)
        TEST_ASSERT_EQUAL(0, e.update(three(t, true, true, true), r).size());
    out = e.update(three(420, true, true, true), r);
    TEST_ASSERT_EQUAL(1, out.size());
    TEST_ASSERT_TRUE(out[0].type == AlertType::SensorRecovered);
    TEST_ASSERT_EQUAL_STRING("Sensors recovered", out[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("Light sensor, IR sky sensor, Environment sensor are reporting normally again.", out[0].message.c_str());
    TEST_ASSERT_EQUAL_STRING("Light sensor, IR sky sensor, Environment sensor", var(out[0], "sensor").c_str());
}

void test_sensors_faulting_at_different_times_alert_one_by_one(void)
{
    AlertEngine e;
    const AlertRules r = rules();
    e.update(three(0, true, true, true), r);
    std::vector<Alert> sent;
    for (uint32_t t = 10; t <= 200; t += 10)
    {
        const std::vector<Alert> out = e.update(three(t, false, t < 50, true), r); // IR fails 40 s later
        sent.insert(sent.end(), out.begin(), out.end());
    }
    TEST_ASSERT_EQUAL(2, sent.size());
    TEST_ASSERT_EQUAL_STRING("Sensor fault: Light sensor", sent[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("Light sensor is offline or reporting errors.", sent[0].message.c_str());
    TEST_ASSERT_EQUAL_STRING("Light sensor", var(sent[0], "sensor").c_str());
    TEST_ASSERT_EQUAL_STRING("Sensor fault: IR sky sensor", sent[1].title.c_str());
}

void run_sensor_fault_delay_tests(void)
{
    RUN_TEST(test_several_sensors_faulting_together_send_one_grouped_alert);
    RUN_TEST(test_sensors_faulting_at_different_times_alert_one_by_one);
    RUN_TEST(test_sensor_fault_shorter_than_the_delay_sends_nothing);
    RUN_TEST(test_sensor_fault_longer_than_the_delay_sends_one_fault_then_one_recovery);
    RUN_TEST(test_sensor_fault_delay_zero_alerts_at_once_and_recovery_waits_the_cooldown);
    RUN_TEST(test_sensor_fault_from_boot_is_held_by_the_startup_grace);
    RUN_TEST(test_sensor_recovery_only_follows_an_announced_fault);
    RUN_TEST(test_lens_fault_waits_the_sensor_fault_delay);
    RUN_TEST(test_rain_started_on_the_first_drops_has_no_rate_and_is_not_delayed);
    RUN_TEST(test_rain_started_with_a_rate_reports_it);
}
