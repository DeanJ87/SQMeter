#include <unity.h>

#include "BatteryLogic.h"

// The power policy (specs/032-battery research D7).

using namespace SQM::Battery;

namespace
{
    PolicyInput read(uint32_t now, float percent, ChargeState state = ChargeState::Discharging)
    {
        PolicyInput in;
        in.newRead = true;
        in.valid = true;
        in.percent = percent;
        in.state = state;
        in.usbPower = state == ChargeState::Charging || state == ChargeState::Charged;
        in.nowMs = now;
        return in;
    }

    PolicyInput tick(uint32_t now, bool idle = true)
    {
        PolicyInput in;
        in.newRead = false;
        in.state = ChargeState::Discharging;
        in.usbPower = false;
        in.dispatcherIdle = idle;
        in.nowMs = now;
        return in;
    }

    // Saving, then two reads at 4 %: shutdown pending from `at`.
    void toPending(PowerPolicy &p, uint32_t at)
    {
        p.update(read(at - 20000, 9.0f));
        p.update(read(at - 10000, 4.0f));
        TEST_ASSERT_TRUE(p.update(read(at, 4.0f)).shutdownStarted);
    }
} // namespace

void test_saving_below_critical(void)
{
    PowerPolicy p;
    TEST_ASSERT_FALSE(p.update(read(0, 10.0f)).enteredSaving);
    TEST_ASSERT_TRUE(p.state() == PowerState::Normal);
    PolicyEvents e = p.update(read(5000, 9.9f));
    TEST_ASSERT_TRUE(e.enteredSaving);
    TEST_ASSERT_TRUE(p.state() == PowerState::Saving);
    // Not while charging.
    PowerPolicy charging;
    TEST_ASSERT_FALSE(charging.update(read(0, 5.0f, ChargeState::Charging)).enteredSaving);
    TEST_ASSERT_TRUE(charging.state() == PowerState::Normal);
}

void test_saving_back_to_normal(void)
{
    PowerPolicy p;
    p.update(read(0, 9.0f));
    TEST_ASSERT_FALSE(p.update(read(5000, 14.9f)).powerRestored);
    PolicyEvents e = p.update(read(10000, 15.0f));
    TEST_ASSERT_TRUE(e.powerRestored);
    TEST_ASSERT_TRUE(p.state() == PowerState::Normal);

    p.update(read(15000, 9.0f));
    e = p.update(read(20000, 9.0f, ChargeState::Charging));
    TEST_ASSERT_TRUE(e.powerRestored);
    TEST_ASSERT_TRUE(p.state() == PowerState::Normal);

    // USB without a charging state (pins say PG).
    p.update(read(25000, 9.0f));
    PolicyInput usb = read(30000, 9.0f, ChargeState::NoBattery);
    usb.usbPower = true;
    TEST_ASSERT_TRUE(p.update(usb).powerRestored);
}

void test_shutdown_needs_two_valid_reads(void)
{
    PowerPolicy p;
    p.update(read(0, 9.0f));
    TEST_ASSERT_FALSE(p.update(read(10000, 5.0f)).shutdownStarted);
    TEST_ASSERT_FALSE(p.update(tick(12000)).shutdownStarted); // no read: no count
    TEST_ASSERT_FALSE(p.update(read(20000, 5.1f)).shutdownStarted); // above: reset
    TEST_ASSERT_FALSE(p.update(read(30000, 5.0f)).shutdownStarted);
    PolicyEvents e = p.update(read(40000, 4.8f));
    TEST_ASSERT_TRUE(e.shutdownStarted);
    TEST_ASSERT_TRUE(p.state() == PowerState::ShutdownPending);
    TEST_ASSERT_EQUAL_UINT32(30, p.secondsLeft(40000, 30));
    TEST_ASSERT_EQUAL_UINT32(20, p.secondsLeft(50000, 30));
}

void test_sleep_after_grace_and_idle_dispatcher(void)
{
    PowerPolicy p;
    toPending(p, 100000);
    TEST_ASSERT_FALSE(p.update(tick(129999)).sleepNow);
    // Grace over but an alert still going out: wait, at most 15 s more.
    TEST_ASSERT_FALSE(p.update(tick(130000, false)).sleepNow);
    TEST_ASSERT_FALSE(p.update(tick(144999, false)).sleepNow);
    TEST_ASSERT_TRUE(p.update(tick(145000, false)).sleepNow);

    PowerPolicy idle;
    toPending(idle, 100000);
    TEST_ASSERT_TRUE(idle.update(tick(130000, true)).sleepNow);
}

void test_usb_during_grace_cancels(void)
{
    PowerPolicy p;
    toPending(p, 100000);
    PolicyEvents e = p.update(read(110000, 4.0f, ChargeState::Charging));
    TEST_ASSERT_TRUE(e.powerRestored);
    TEST_ASSERT_FALSE(e.sleepNow);
    TEST_ASSERT_TRUE(p.state() == PowerState::Normal);
    TEST_ASSERT_EQUAL_UINT32(0, p.secondsLeft(110000, 30));
}

void test_missing_data_never_moves_towards_sleep(void)
{
    PowerPolicy p;
    PolicyInput bad = read(0, 1.0f);
    bad.valid = false;
    for (uint32_t t = 0; t < 100000; t += 5000)
    {
        bad.nowMs = t;
        const PolicyEvents e = p.update(bad);
        TEST_ASSERT_FALSE(e.enteredSaving || e.shutdownStarted || e.sleepNow);
    }
    TEST_ASSERT_TRUE(p.state() == PowerState::Normal);
    p.update(read(100000, 9.0f));
    for (uint32_t t = 105000; t < 200000; t += 5000)
    {
        bad.nowMs = t;
        TEST_ASSERT_FALSE(p.update(bad).shutdownStarted);
    }
    TEST_ASSERT_TRUE(p.state() == PowerState::Saving);
}

void test_power_saving_off_stays_normal(void)
{
    PowerPolicy p;
    PolicyInput in = read(0, 3.0f);
    in.powerSaving = false;
    for (uint32_t t = 0; t < 60000; t += 5000)
    {
        in.nowMs = t;
        const PolicyEvents e = p.update(in);
        TEST_ASSERT_FALSE(e.enteredSaving || e.shutdownStarted || e.sleepNow);
    }
    TEST_ASSERT_TRUE(p.state() == PowerState::Normal);
}

void test_policy_survives_millis_wrap(void)
{
    PowerPolicy p;
    const uint32_t start = 0xFFFFFFFFu - 10000u;
    toPending(p, start);
    TEST_ASSERT_FALSE(p.update(tick(start + 20000u)).sleepNow); // wrapped, 20 s in
    TEST_ASSERT_TRUE(p.update(tick(start + 30000u)).sleepNow);
}

void run_power_policy_tests(void)
{
    RUN_TEST(test_saving_below_critical);
    RUN_TEST(test_saving_back_to_normal);
    RUN_TEST(test_shutdown_needs_two_valid_reads);
    RUN_TEST(test_sleep_after_grace_and_idle_dispatcher);
    RUN_TEST(test_usb_during_grace_cancels);
    RUN_TEST(test_missing_data_never_moves_towards_sleep);
    RUN_TEST(test_power_saving_off_stays_normal);
    RUN_TEST(test_policy_survives_millis_wrap);
}
