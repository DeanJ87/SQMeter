#include <unity.h>

#include "StartupSequence.h"

#include <utility>
#include <vector>

// Staged start-up after the core is up: the firmware is confirmed before
// anything heavy starts, then one step at a time with a pause between them,
// so the start-up current peaks don't stack up and brown out a weak supply.

using namespace SQM::Boot;

void setUp() {}
void tearDown() {}

namespace
{
    using Timeline = std::vector<std::pair<uint32_t, Step>>;

    // Drives the sequence like the main loop, a pass every 10 ms, until
    // everything has run (or `untilMs`).
    Timeline run(StartupSequence &sequence, uint32_t startMs = 1000, uint32_t untilMs = 60000)
    {
        Timeline timeline;
        sequence.start(startMs);
        for (uint32_t now = startMs; now <= untilMs && !sequence.finished(); now += 10)
        {
            if (const auto step = sequence.next(now))
                timeline.emplace_back(now - startMs, *step);
        }
        return timeline;
    }

    size_t indexOf(const Timeline &timeline, Step step)
    {
        for (size_t i = 0; i < timeline.size(); ++i)
            if (timeline[i].second == step)
                return i;
        return timeline.size();
    }

    uint32_t timeOf(const Timeline &timeline, Step step)
    {
        return timeline.at(indexOf(timeline, step)).first;
    }

    constexpr Step SENSORS[] = {
        Step::Rain, Step::SkySensors, Step::Infrared, Step::Environment, Step::Gps, Step::Wind, Step::Battery, Step::Display};
    constexpr Step RADIO_AND_NETWORK[] = {Step::Time, Step::Mqtt, Step::Mdns, Step::AlpacaDiscovery, Step::Bluetooth, Step::WifiFullPower};
} // namespace

void test_confirmation_is_due_at_once_when_started()
{
    StartupSequence sequence;
    sequence.start(5000);
    const auto first = sequence.next(5000);
    TEST_ASSERT_TRUE(first.has_value());
    TEST_ASSERT_EQUAL(static_cast<int>(Step::ConfirmImage), static_cast<int>(*first));
    TEST_ASSERT_TRUE(sequence.done(Step::ConfirmImage));
}

void test_nothing_runs_before_start()
{
    StartupSequence sequence;
    TEST_ASSERT_FALSE(sequence.next(0).has_value());
    TEST_ASSERT_FALSE(sequence.next(100000).has_value());
    TEST_ASSERT_FALSE(sequence.done(Step::ConfirmImage));
    TEST_ASSERT_FALSE(sequence.done(Step::Rain));
}

void test_confirmation_comes_before_every_sensor_and_radio()
{
    StartupSequence sequence;
    const Timeline timeline = run(sequence);
    TEST_ASSERT_EQUAL(0, indexOf(timeline, Step::ConfirmImage));
    TEST_ASSERT_EQUAL_UINT32(0, timeOf(timeline, Step::ConfirmImage));
    for (Step step : SENSORS)
        TEST_ASSERT_GREATER_THAN_UINT32(timeOf(timeline, Step::ConfirmImage), timeOf(timeline, step));
    for (Step step : RADIO_AND_NETWORK)
        TEST_ASSERT_GREATER_THAN_UINT32(timeOf(timeline, Step::ConfirmImage), timeOf(timeline, step));
}

void test_every_step_runs_once_in_order()
{
    StartupSequence sequence;
    const Timeline timeline = run(sequence);
    TEST_ASSERT_EQUAL(STEP_COUNT, timeline.size());
    for (size_t i = 0; i < timeline.size(); ++i)
        TEST_ASSERT_EQUAL(static_cast<int>(i), static_cast<int>(timeline[i].second));
    TEST_ASSERT_TRUE(sequence.finished());
    TEST_ASSERT_FALSE(sequence.next(100000).has_value());
}

void test_sensors_start_before_the_network_services_and_bluetooth_last()
{
    StartupSequence sequence;
    const Timeline timeline = run(sequence);
    for (Step sensor : SENSORS)
        TEST_ASSERT_LESS_THAN(indexOf(timeline, Step::Time), indexOf(timeline, sensor));
    TEST_ASSERT_EQUAL(1, indexOf(timeline, Step::Rain)); // the safety input first
    TEST_ASSERT_LESS_THAN(indexOf(timeline, Step::Bluetooth), indexOf(timeline, Step::Mqtt));
    TEST_ASSERT_EQUAL(timeline.size() - 1, indexOf(timeline, Step::WifiFullPower));
}

void test_steps_are_spaced_by_their_pauses()
{
    StartupSequence sequence;
    const Timeline timeline = run(sequence);
    TEST_ASSERT_EQUAL_UINT32(SETTLE_AFTER_CONFIRM_MS, timeOf(timeline, Step::Rain));
    for (size_t i = 2; i < timeline.size(); ++i)
    {
        const uint32_t pause = timeline[i].first - timeline[i - 1].first;
        TEST_ASSERT_EQUAL_UINT32(gapBeforeMs(timeline[i].second), pause);
    }
    TEST_ASSERT_EQUAL_UINT32(BLUETOOTH_GAP_MS, timeOf(timeline, Step::Bluetooth) - timeOf(timeline, Step::AlpacaDiscovery));
}

void test_no_step_runs_a_millisecond_early()
{
    StartupSequence sequence;
    sequence.start(0);
    TEST_ASSERT_TRUE(sequence.next(0).has_value()); // confirm
    TEST_ASSERT_FALSE(sequence.next(SETTLE_AFTER_CONFIRM_MS - 1).has_value());
    TEST_ASSERT_TRUE(sequence.next(SETTLE_AFTER_CONFIRM_MS).has_value()); // rain
    TEST_ASSERT_FALSE(sequence.next(SETTLE_AFTER_CONFIRM_MS + STEP_GAP_MS - 1).has_value());
    TEST_ASSERT_TRUE(sequence.next(SETTLE_AFTER_CONFIRM_MS + STEP_GAP_MS).has_value()); // sky
}

void test_all_sensors_are_up_within_a_few_seconds()
{
    StartupSequence sequence;
    const Timeline timeline = run(sequence);
    TEST_ASSERT_EQUAL_UINT32(2250, timeOf(timeline, Step::Display));
    TEST_ASSERT_EQUAL_UINT32(plannedMsUntil(ALL_STEPS, Step::Display), timeOf(timeline, Step::Display));
    TEST_ASSERT_EQUAL_UINT32(4500, timeOf(timeline, Step::WifiFullPower));
    TEST_ASSERT_EQUAL_UINT32(plannedMs(ALL_STEPS), timeOf(timeline, Step::WifiFullPower));
}

void test_steps_left_out_are_passed_over_without_a_pause()
{
    const uint32_t steps = ALL_STEPS & ~stepBit(Step::Gps) & ~stepBit(Step::Rain) & ~stepBit(Step::Bluetooth);
    StartupSequence sequence(steps);
    const Timeline timeline = run(sequence);
    TEST_ASSERT_EQUAL(STEP_COUNT - 3, timeline.size());
    TEST_ASSERT_EQUAL(timeline.size(), indexOf(timeline, Step::Gps));
    TEST_ASSERT_EQUAL(timeline.size(), indexOf(timeline, Step::Bluetooth));
    // The sky sensor is now first after the confirmation, after the settle pause.
    TEST_ASSERT_EQUAL_UINT32(SETTLE_AFTER_CONFIRM_MS, timeOf(timeline, Step::SkySensors));
    TEST_ASSERT_EQUAL_UINT32(STEP_GAP_MS, timeOf(timeline, Step::Wind) - timeOf(timeline, Step::Environment));
    TEST_ASSERT_EQUAL_UINT32(STEP_GAP_MS, timeOf(timeline, Step::WifiFullPower) - timeOf(timeline, Step::AlpacaDiscovery));
    TEST_ASSERT_TRUE(sequence.done(Step::Gps));
    TEST_ASSERT_TRUE(sequence.done(Step::Bluetooth));
    TEST_ASSERT_EQUAL_UINT32(plannedMs(steps), timeOf(timeline, Step::WifiFullPower));
}

void test_confirmation_and_full_power_cannot_be_left_out()
{
    StartupSequence sequence(0);
    const Timeline timeline = run(sequence);
    TEST_ASSERT_EQUAL(2, timeline.size());
    TEST_ASSERT_EQUAL(static_cast<int>(Step::ConfirmImage), static_cast<int>(timeline[0].second));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::WifiFullPower), static_cast<int>(timeline[1].second));
    TEST_ASSERT_EQUAL_UINT32(SETTLE_AFTER_CONFIRM_MS, timeline[1].first);
}

void test_done_reports_only_steps_already_run()
{
    StartupSequence sequence;
    sequence.start(0);
    sequence.next(0);
    sequence.next(SETTLE_AFTER_CONFIRM_MS);
    TEST_ASSERT_TRUE(sequence.done(Step::Rain));
    TEST_ASSERT_FALSE(sequence.done(Step::SkySensors));
    TEST_ASSERT_FALSE(sequence.done(Step::Mqtt));
}

void test_a_slow_step_still_gets_a_full_pause_after_it()
{
    StartupSequence sequence;
    sequence.start(0);
    sequence.next(0);
    TEST_ASSERT_TRUE(sequence.next(SETTLE_AFTER_CONFIRM_MS).has_value());
    // The rain sensor took two seconds to answer.
    const uint32_t finishedAt = SETTLE_AFTER_CONFIRM_MS + 2000;
    sequence.stepFinished(finishedAt);
    TEST_ASSERT_FALSE(sequence.next(finishedAt).has_value());
    TEST_ASSERT_FALSE(sequence.next(finishedAt + STEP_GAP_MS - 1).has_value());
    TEST_ASSERT_TRUE(sequence.next(finishedAt + STEP_GAP_MS).has_value());
}

void test_millis_rollover_does_not_stall_the_sequence()
{
    StartupSequence sequence;
    const uint32_t start = 0xFFFFFF00UL;
    sequence.start(start);
    TEST_ASSERT_TRUE(sequence.next(start).has_value());
    TEST_ASSERT_TRUE(sequence.next(start + SETTLE_AFTER_CONFIRM_MS).has_value()); // wraps past zero
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_confirmation_is_due_at_once_when_started);
    RUN_TEST(test_nothing_runs_before_start);
    RUN_TEST(test_confirmation_comes_before_every_sensor_and_radio);
    RUN_TEST(test_every_step_runs_once_in_order);
    RUN_TEST(test_sensors_start_before_the_network_services_and_bluetooth_last);
    RUN_TEST(test_steps_are_spaced_by_their_pauses);
    RUN_TEST(test_no_step_runs_a_millisecond_early);
    RUN_TEST(test_all_sensors_are_up_within_a_few_seconds);
    RUN_TEST(test_steps_left_out_are_passed_over_without_a_pause);
    RUN_TEST(test_confirmation_and_full_power_cannot_be_left_out);
    RUN_TEST(test_done_reports_only_steps_already_run);
    RUN_TEST(test_a_slow_step_still_gets_a_full_pause_after_it);
    RUN_TEST(test_millis_rollover_does_not_stall_the_sequence);
    return UNITY_END();
}
