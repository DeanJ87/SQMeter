#include <unity.h>

#include "calculations/SkyReadCycle.h"

using namespace SQM;

namespace
{
    using Action = SkyReadCycle::Action;
    using State = SkyReadCycle::State;
    constexpr uint16_t INTEGRATION = 600;
    constexpr uint32_t READY = SkyReadCycle::collectDelayMs(INTEGRATION);

    // A cycle that has been begun successfully at `now`.
    SkyReadCycle begun(uint32_t now)
    {
        SkyReadCycle cycle(650);
        TEST_ASSERT_TRUE(cycle.next(now, INTEGRATION) == Action::Begin);
        cycle.beginDone(now, true);
        return cycle;
    }
} // namespace

void test_read_cycle_never_begun_starts_absent_and_tries_at_once()
{
    SkyReadCycle cycle(650);
    TEST_ASSERT_TRUE(cycle.state() == State::Absent);
    TEST_ASSERT_FALSE(cycle.initialized());
    TEST_ASSERT_TRUE(cycle.next(0, INTEGRATION) == Action::Begin);
    TEST_ASSERT_TRUE(cycle.next(123456, INTEGRATION) == Action::Begin);

    // Not found: retry after 5 s, then 10, 20, 40, 60, 60 (cap).
    uint32_t now = 1000;
    const uint32_t waits[] = {5000, 10000, 20000, 40000, 60000, 60000};
    for (uint32_t wait : waits)
    {
        cycle.beginDone(now, false);
        TEST_ASSERT_TRUE(cycle.state() == State::Absent);
        TEST_ASSERT_TRUE(cycle.next(now + wait - 1, INTEGRATION) == Action::None);
        TEST_ASSERT_TRUE(cycle.next(now + wait, INTEGRATION) == Action::Begin);
        now += wait;
    }
}

void test_read_cycle_starts_when_due_and_collects_after_integration()
{
    SkyReadCycle cycle = begun(1000);
    TEST_ASSERT_TRUE(cycle.state() == State::Idle);
    TEST_ASSERT_TRUE(cycle.initialized());
    // Due at once after begin.
    TEST_ASSERT_TRUE(cycle.next(1000, INTEGRATION) == Action::Start);
    cycle.started(1000);
    TEST_ASSERT_TRUE(cycle.state() == State::Integrating);
    TEST_ASSERT_TRUE(cycle.next(1000 + READY - 1, INTEGRATION) == Action::None);
    TEST_ASSERT_TRUE(cycle.next(1000 + READY, INTEGRATION) == Action::Collect);
    cycle.collected(1000 + READY, true);
    TEST_ASSERT_TRUE(cycle.state() == State::Idle);

    // Next start at once: the cadence (650 ms) has passed while integrating.
    TEST_ASSERT_TRUE(cycle.next(1000 + READY, INTEGRATION) == Action::Start);

    // The wait follows the integration time (auto-ranging): 600 ms -> 740 ms, 300 ms -> 380 ms.
    TEST_ASSERT_EQUAL_UINT32(740, READY);
    TEST_ASSERT_EQUAL_UINT32(380, SkyReadCycle::collectDelayMs(300));
    cycle.started(2000);
    TEST_ASSERT_TRUE(cycle.next(2000 + 379, 300) == Action::None);
    TEST_ASSERT_TRUE(cycle.next(2000 + 380, 300) == Action::Collect);
}

void test_read_cycle_failures_go_absent_with_backoff()
{
    SkyReadCycle cycle = begun(0);
    uint32_t now = 0;
    for (uint8_t i = 1; i <= SkyReadCycle::REBEGIN_AFTER; ++i)
    {
        TEST_ASSERT_TRUE(cycle.next(now, INTEGRATION) == Action::Start);
        cycle.started(now);
        now += READY;
        cycle.collected(now, false);
        TEST_ASSERT_EQUAL_UINT8(i, cycle.consecutiveFailures());
        if (i < SkyReadCycle::REBEGIN_AFTER)
        {
            TEST_ASSERT_TRUE(cycle.state() == State::Idle);
            TEST_ASSERT_TRUE(cycle.initialized());
        }
        now += 650;
    }
    now -= 650;
    TEST_ASSERT_TRUE(cycle.state() == State::Absent);
    TEST_ASSERT_FALSE(cycle.initialized());
    TEST_ASSERT_TRUE(cycle.next(now + 4999, INTEGRATION) == Action::None);
    TEST_ASSERT_TRUE(cycle.next(now + 5000, INTEGRATION) == Action::Begin);
    cycle.beginDone(now + 5000, false);
    TEST_ASSERT_TRUE(cycle.next(now + 5000 + 9999, INTEGRATION) == Action::None);
    TEST_ASSERT_TRUE(cycle.next(now + 5000 + 10000, INTEGRATION) == Action::Begin);

    // Replugged: begin works, back to reading at once with the count reset.
    cycle.beginDone(now + 15000, true);
    TEST_ASSERT_TRUE(cycle.state() == State::Idle);
    TEST_ASSERT_EQUAL_UINT8(0, cycle.consecutiveFailures());
    TEST_ASSERT_TRUE(cycle.next(now + 15000, INTEGRATION) == Action::Start);

    // Back-off restarts at 5 s after the next run of failures.
    uint32_t t = now + 15000;
    for (uint8_t i = 0; i < SkyReadCycle::REBEGIN_AFTER; ++i)
    {
        cycle.started(t);
        cycle.collected(t + READY, false);
        t += 1000;
    }
    t = t - 1000 + READY;
    TEST_ASSERT_TRUE(cycle.next(t + 4999, INTEGRATION) == Action::None);
    TEST_ASSERT_TRUE(cycle.next(t + 5000, INTEGRATION) == Action::Begin);
}

void test_read_cycle_success_resets_failures()
{
    SkyReadCycle cycle = begun(0);
    cycle.started(0);
    cycle.collected(READY, false);
    cycle.started(1000);
    cycle.collected(1000 + READY, false);
    TEST_ASSERT_EQUAL_UINT8(2, cycle.consecutiveFailures());
    cycle.started(2000);
    cycle.collected(2000 + READY, true);
    TEST_ASSERT_EQUAL_UINT8(0, cycle.consecutiveFailures());
    cycle.started(3000);
    cycle.collected(3000 + READY, false);
    TEST_ASSERT_TRUE(cycle.state() == State::Idle);

    // A failed start counts as a failed read.
    cycle.startFailed(4000);
    cycle.startFailed(5000);
    TEST_ASSERT_TRUE(cycle.state() == State::Absent);
}

void test_read_cycle_millis_wrap()
{
    const uint32_t nearWrap = 0xFFFFFFFFu - 100;
    SkyReadCycle cycle = begun(nearWrap);
    TEST_ASSERT_TRUE(cycle.next(nearWrap, INTEGRATION) == Action::Start);
    cycle.started(nearWrap);
    const uint32_t ready = nearWrap + READY; // wraps past 0
    TEST_ASSERT_TRUE(cycle.next(ready - 1, INTEGRATION) == Action::None);
    TEST_ASSERT_TRUE(cycle.next(ready, INTEGRATION) == Action::Collect);
    cycle.collected(ready, true);
    TEST_ASSERT_TRUE(cycle.next(ready, INTEGRATION) == Action::Start);
    // With a short integration the cadence holds the next start back.
    cycle.started(ready);
    cycle.collected(ready + SkyReadCycle::collectDelayMs(100), true);
    TEST_ASSERT_TRUE(cycle.next(ready + 649, 100) == Action::None);
    TEST_ASSERT_TRUE(cycle.next(ready + 650, 100) == Action::Start);

    // Back-off across the wrap.
    SkyReadCycle absent(650);
    absent.beginDone(nearWrap, false);
    TEST_ASSERT_TRUE(absent.next(nearWrap + 4999, INTEGRATION) == Action::None);
    TEST_ASSERT_TRUE(absent.next(nearWrap + 5000, INTEGRATION) == Action::Begin);
}
