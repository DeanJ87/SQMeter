#include <unity.h>

#include "AlertEngine.h"

// The dew-suspected alert (specs/036-rain-needs-cloud FR-010): off unless
// chosen, after the sensor fault delay, once per spell.

using namespace SQM::Alerts;

namespace
{
    AlertRules dewRules(bool on = true)
    {
        AlertRules r; // sensorFaultDelaySeconds keeps its default, 120 s
        r.startupGraceSeconds = 0;
        r.cooldownSeconds = 60;
        r.onRain = false;
        r.onDewSuspected = on;
        return r;
    }

    AlertInputs dewAt(uint32_t now, bool dew)
    {
        AlertInputs in;
        in.nowSeconds = now;
        in.rainEnabled = true;
        in.raining = dew;
        in.rainRateMmPerHour = 0.12f;
        in.dewSuspected = dew;
        in.dewCloudCoverPercent = 0.0f;
        return in;
    }

    // Runs the engine every 10 s over [from, to) and counts dew-suspected alerts.
    int dewAlerts(AlertEngine &e, const AlertRules &r, uint32_t from, uint32_t to, bool dew)
    {
        int n = 0;
        for (uint32_t t = from; t < to; t += 10)
            for (const Alert &a : e.update(dewAt(t, dew), r))
                n += a.type == AlertType::DewSuspected ? 1 : 0;
        return n;
    }
} // namespace

void test_dew_suspected_alert_after_the_delay_once_per_spell(void)
{
    AlertEngine e;
    const AlertRules r = dewRules();
    e.update(dewAt(0, false), r);
    TEST_ASSERT_EQUAL(0, dewAlerts(e, r, 10, 130, true)); // waits 120 s
    TEST_ASSERT_EQUAL(1, dewAlerts(e, r, 130, 900, true));
    TEST_ASSERT_EQUAL(0, dewAlerts(e, r, 900, 1200, false)); // clearing sends nothing
    TEST_ASSERT_EQUAL(1, dewAlerts(e, r, 1200, 1500, true)); // the next spell alerts again
}

void test_dew_suspected_shorter_than_the_delay_sends_nothing(void)
{
    AlertEngine e;
    const AlertRules r = dewRules();
    e.update(dewAt(0, false), r);
    TEST_ASSERT_EQUAL(0, dewAlerts(e, r, 10, 60, true));
    TEST_ASSERT_EQUAL(0, dewAlerts(e, r, 60, 400, false));
}

void test_dew_suspected_off_sends_nothing(void)
{
    AlertEngine e;
    const AlertRules r = dewRules(false);
    TEST_ASSERT_EQUAL(0, dewAlerts(e, r, 0, 900, true));
}

void test_dew_suspected_wording(void)
{
    AlertEngine e;
    const AlertRules r = dewRules();
    e.update(dewAt(0, false), r);
    dewAlerts(e, r, 10, 130, true);
    const std::vector<Alert> out = e.update(dewAt(130, true), r);
    TEST_ASSERT_EQUAL(1, out.size());
    TEST_ASSERT_EQUAL_STRING("dew_suspected", alertTypeName(out[0].type));
    TEST_ASSERT_EQUAL_STRING("Rain sensor: dew suspected", out[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("Rain 0.12 mm/h under a clear sky (cloud 0%). Check the rain sensor's lens.", out[0].message.c_str());
}

void run_dew_suspected_tests(void)
{
    RUN_TEST(test_dew_suspected_alert_after_the_delay_once_per_spell);
    RUN_TEST(test_dew_suspected_shorter_than_the_delay_sends_nothing);
    RUN_TEST(test_dew_suspected_off_sends_nothing);
    RUN_TEST(test_dew_suspected_wording);
}
