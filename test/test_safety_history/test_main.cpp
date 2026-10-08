#include <unity.h>

#include <cstring>

#include "SafetyHistoryLog.h"

using namespace SQM::SafetyHistory;

namespace
{
    Entry entry(Kind kind, bool safe, uint32_t uptimeS = 0, uint16_t boot = 1, uint32_t epoch = 0)
    {
        Entry e{};
        e.kind = kind;
        e.safe = safe;
        e.uptimeS = uptimeS;
        e.boot = boot;
        e.epoch = epoch;
        return e;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_garbage_is_cleared()
{
    Log log;
    std::memset(&log, 0xA5, sizeof(log)); // RTC memory after power-on
    TEST_ASSERT_EQUAL(1, startBoot(log, true));
    TEST_ASSERT_EQUAL(0, log.count);
    TEST_ASSERT_EQUAL_HEX32(MAGIC, log.magic);
}

void test_restart_keeps_history_power_cut_clears()
{
    Log log{};
    startBoot(log, true);
    push(log, entry(Kind::Alert, false));
    TEST_ASSERT_EQUAL(2, startBoot(log, true)); // software restart
    TEST_ASSERT_EQUAL(1, log.count);
    TEST_ASSERT_EQUAL(1, startBoot(log, false)); // power-on / brownout
    TEST_ASSERT_EQUAL(0, log.count);
}

void test_ring_keeps_newest()
{
    Log log{};
    startBoot(log, false);
    for (uint32_t i = 0; i < CAPACITY + 5; ++i)
        push(log, entry(Kind::Change, i % 2 == 0, i));
    TEST_ASSERT_EQUAL(CAPACITY, log.count);

    Entry out[CAPACITY];
    TEST_ASSERT_EQUAL(CAPACITY, copy(log, out, CAPACITY));
    TEST_ASSERT_EQUAL(5, out[0].uptimeS); // oldest kept
    TEST_ASSERT_EQUAL(CAPACITY + 4, out[CAPACITY - 1].uptimeS);

    Entry few[3];
    TEST_ASSERT_EQUAL(3, copy(log, few, 3));
    TEST_ASSERT_EQUAL(CAPACITY + 2, few[0].uptimeS);
    TEST_ASSERT_EQUAL(CAPACITY + 4, few[2].uptimeS);
}

void test_last_alert_across_boots()
{
    Log log{};
    startBoot(log, false);
    bool safe = true;
    TEST_ASSERT_FALSE(lastAlert(log, safe));

    push(log, entry(Kind::Alert, false));
    push(log, entry(Kind::Change, true));
    push(log, entry(Kind::Armed, true));
    startBoot(log, true);
    push(log, entry(Kind::Boot, false, 0, 2));
    // The newest alert said unsafe, even though later entries are "safe".
    TEST_ASSERT_TRUE(lastAlert(log, safe));
    TEST_ASSERT_FALSE(safe);

    push(log, entry(Kind::Alert, true, 10, 2));
    TEST_ASSERT_TRUE(lastAlert(log, safe));
    TEST_ASSERT_TRUE(safe);
}

void test_last_alert_after_wrap()
{
    Log log{};
    startBoot(log, false);
    push(log, entry(Kind::Alert, true));
    for (uint32_t i = 0; i < CAPACITY - 1; ++i)
        push(log, entry(Kind::Change, false));
    bool safe = false;
    TEST_ASSERT_TRUE(lastAlert(log, safe)); // still the oldest slot
    TEST_ASSERT_TRUE(safe);
    push(log, entry(Kind::Change, false)); // overwrites it
    TEST_ASSERT_FALSE(lastAlert(log, safe));
}

void test_backfill_epochs()
{
    Entry entries[3] = {
        entry(Kind::Boot, false, 0, 1),        // earlier boot: can't be dated
        entry(Kind::Boot, false, 2, 2),        // this boot, before the clock
        entry(Kind::Change, true, 50, 2, 777), // already dated
    };
    backfillEpochs(entries, 3, 2, 1000000, 100);
    TEST_ASSERT_EQUAL(0, entries[0].epoch);
    TEST_ASSERT_EQUAL(1000000 - 98, entries[1].epoch);
    TEST_ASSERT_EQUAL(777, entries[2].epoch);

    Entry unset = entry(Kind::Boot, false, 2, 2);
    backfillEpochs(&unset, 1, 2, 0, 100); // clock still not set
    TEST_ASSERT_EQUAL(0, unset.epoch);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_garbage_is_cleared);
    RUN_TEST(test_restart_keeps_history_power_cut_clears);
    RUN_TEST(test_ring_keeps_newest);
    RUN_TEST(test_last_alert_across_boots);
    RUN_TEST(test_last_alert_after_wrap);
    RUN_TEST(test_backfill_epochs);
    return UNITY_END();
}
