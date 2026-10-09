#include <unity.h>

#include "RainLogic.h"

using namespace SQM;

void setUp() {}
void tearDown() {}

void test_parses_metric_line()
{
    Rain::Line line;
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::Ok),
        static_cast<int>(Rain::parseLine("Acc  0.01 mm, EventAcc  0.20 mm, TotalAcc 12.60 mm, RInt  0.47 mmph", line)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.01f, line.acc);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.20f, line.eventAcc);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.60f, line.totalAcc);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.47f, line.rInt);
    TEST_ASSERT_FALSE(line.imperial);
    TEST_ASSERT_FALSE(line.lensBad);
    TEST_ASSERT_FALSE(line.emSat);
}

void test_parses_imperial_and_flags()
{
    Rain::Line line;
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::Ok),
        static_cast<int>(Rain::parseLine("Acc 0.001 in, EventAcc 0.010 in, TotalAcc 0.500 in, RInt 0.020 iph i o", line)));
    TEST_ASSERT_TRUE(line.imperial);
    TEST_ASSERT_TRUE(line.lensBad);
    TEST_ASSERT_TRUE(line.emSat);

    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::Ok),
        static_cast<int>(Rain::parseLine("Acc 0.00 mm, EventAcc 0.00 mm, TotalAcc 1.00 mm, RInt 0.00 mmph LensBad", line)));
    TEST_ASSERT_TRUE(line.lensBad);
    TEST_ASSERT_FALSE(line.emSat);
}

void test_rejects_garbled_lines()
{
    Rain::Line line;
    line.totalAcc = 42.0f;
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ParseResult::TooShort), static_cast<int>(Rain::parseLine("Acc 0.01 mm", line)));
    // Truncated mid-line: RInt missing.
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::MissingField),
        static_cast<int>(Rain::parseLine("Acc 0.01 mm, EventAcc 0.20 mm, TotalAcc 12.6", line)));
    // Corrupt number.
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::MissingField),
        static_cast<int>(Rain::parseLine("Acc x.01 mm, EventAcc 0.20 mm, TotalAcc 12.60 mm, RInt 0.47 mmph", line)));
    // "Acc" only inside "EventAcc" doesn't count as the Acc field.
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::MissingField),
        static_cast<int>(Rain::parseLine("EventAcc 0.20 mm, TotalAcc 12.60 mm, RInt 0.47 mmph", line)));
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::OutOfRange),
        static_cast<int>(Rain::parseLine("Acc -1.00 mm, EventAcc 0.20 mm, TotalAcc 12.60 mm, RInt 0.47 mmph", line)));
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ParseResult::OutOfRange),
        static_cast<int>(Rain::parseLine("Acc 0.00 mm, EventAcc 0.20 mm, TotalAcc 12.60 mm, RInt 99999 mmph", line)));
    // A failed parse leaves the output untouched.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, line.totalAcc);
}

void test_latch_holds_for_clear_delay()
{
    const uint32_t delay = 15 * 60 * 1000;
    Rain::Latch latch;
    Rain::observe(latch, 0.0f, 0.0f, 1000, delay);
    TEST_ASSERT_FALSE(latch.latched);

    Rain::observe(latch, 0.5f, 0.02f, 10000, delay);
    TEST_ASSERT_TRUE(latch.latched);
    Rain::observe(latch, 0.0f, 0.03f, 15000, delay); // accumulation alone counts as rain
    TEST_ASSERT_TRUE(latch.latched);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.05f, latch.eventAccumulation);

    // Dry readings within the delay keep it latched...
    Rain::observe(latch, 0.0f, 0.0f, 15000 + delay, delay);
    TEST_ASSERT_TRUE(latch.latched);
    // ...and it clears just after.
    Rain::observe(latch, 0.0f, 0.0f, 15000 + delay + 1, delay);
    TEST_ASSERT_FALSE(latch.latched);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, latch.eventAccumulation);
}

void test_latch_expires_without_readings()
{
    const uint32_t delay = 60000;
    Rain::Latch latch;
    Rain::observe(latch, 1.0f, 0.1f, 5000, delay);
    Rain::expire(latch, 5000 + delay, delay);
    TEST_ASSERT_TRUE(latch.latched);
    Rain::expire(latch, 5000 + delay + 1, delay);
    TEST_ASSERT_FALSE(latch.latched);
}

void test_new_event_restarts_accumulation()
{
    const uint32_t delay = 1000;
    Rain::Latch latch;
    Rain::observe(latch, 1.0f, 0.4f, 100, delay);
    Rain::observe(latch, 0.0f, 0.0f, 5000, delay);
    TEST_ASSERT_FALSE(latch.latched);
    Rain::observe(latch, 1.0f, 0.1f, 6000, delay);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.1f, latch.eventAccumulation);
}

void test_latch_across_millis_wrap()
{
    const uint32_t delay = 60000;
    Rain::Latch latch;
    Rain::observe(latch, 1.0f, 0.1f, 0xFFFFFF00u, delay);
    Rain::expire(latch, 1000, delay); // ~1.3 s later, after the wrap
    TEST_ASSERT_TRUE(latch.latched);
    Rain::expire(latch, 70000, delay);
    TEST_ASSERT_FALSE(latch.latched);
}

// Daily total reset (spec 003 FR-004): first check at or after HH:MM, once a day.
static Rain::LocalTime at(int year, int yearDay, int hour, int minute)
{
    Rain::LocalTime t;
    t.year = year;
    t.yearDay = yearDay;
    t.hour = hour;
    t.minute = minute;
    return t;
}

static Rain::ResetDecision decide(const Rain::LocalTime &now, int32_t lastDay)
{
    return Rain::dailyReset(now, 9, 0, lastDay); // reset at 09:00
}

void test_daily_reset_adopts_without_resetting_first()
{
    // Nothing recorded (first boot, upgrade): adopt today, keep the total.
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Adopt), static_cast<int>(decide(at(2026, 100, 15, 0), Rain::NO_RESET_DAY)));
}

void test_daily_reset_fires_once_at_or_after_the_time()
{
    const int32_t yesterday = Rain::resetDay(at(2026, 99, 9, 0), 9, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Wait), static_cast<int>(decide(at(2026, 100, 8, 59), yesterday)));
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Reset), static_cast<int>(decide(at(2026, 100, 9, 0), yesterday)));
    const int32_t today = Rain::resetDay(at(2026, 100, 9, 0), 9, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Wait), static_cast<int>(decide(at(2026, 100, 9, 1), today)));
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Wait), static_cast<int>(decide(at(2026, 100, 23, 59), today)));
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Wait), static_cast<int>(decide(at(2026, 101, 8, 59), today)));
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Reset), static_cast<int>(decide(at(2026, 101, 9, 0), today)));
}

void test_daily_reset_survives_a_restart_over_the_minute()
{
    // Down from 08:58 to 09:07: the first check after boot still resets.
    const int32_t yesterday = Rain::resetDay(at(2026, 99, 9, 0), 9, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Reset), static_cast<int>(decide(at(2026, 100, 9, 7), yesterday)));
    // A stall past several days still resets once.
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Reset), static_cast<int>(decide(at(2026, 103, 14, 0), yesterday)));
}

void test_daily_reset_handles_dst_and_clock_steps()
{
    const int32_t yesterday = Rain::resetDay(at(2026, 87, 1, 30), 1, 30);
    // Spring forward 01:00 -> 02:00 skips 01:30: the 02:00 check resets.
    TEST_ASSERT_EQUAL(
        static_cast<int>(Rain::ResetDecision::Reset), static_cast<int>(Rain::dailyReset(at(2026, 88, 2, 0), 1, 30, yesterday)));
    // Fall back repeats 01:30: the second pass doesn't reset again.
    const int32_t today = Rain::resetDay(at(2026, 298, 1, 30), 1, 30);
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Wait), static_cast<int>(Rain::dailyReset(at(2026, 298, 1, 30), 1, 30, today)));
    // The clock steps back a day: no second reset.
    TEST_ASSERT_EQUAL(static_cast<int>(Rain::ResetDecision::Wait), static_cast<int>(Rain::dailyReset(at(2026, 297, 12, 0), 1, 30, today)));
}

void test_reset_day_crosses_years()
{
    // 31 Dec 2025 23:00 and 1 Jan 2026 08:00 are the same reset day (reset 09:00).
    TEST_ASSERT_EQUAL(Rain::resetDay(at(2025, 364, 23, 0), 9, 0), Rain::resetDay(at(2026, 0, 8, 0), 9, 0));
    TEST_ASSERT_EQUAL(Rain::resetDay(at(2025, 364, 23, 0), 9, 0) + 1, Rain::resetDay(at(2026, 0, 9, 0), 9, 0));
    // Leap year: 2024 has 366 days.
    TEST_ASSERT_EQUAL(Rain::resetDay(at(2024, 365, 12, 0), 0, 0) + 1, Rain::resetDay(at(2025, 0, 12, 0), 0, 0));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parses_metric_line);
    RUN_TEST(test_parses_imperial_and_flags);
    RUN_TEST(test_rejects_garbled_lines);
    RUN_TEST(test_latch_holds_for_clear_delay);
    RUN_TEST(test_latch_expires_without_readings);
    RUN_TEST(test_new_event_restarts_accumulation);
    RUN_TEST(test_latch_across_millis_wrap);
    RUN_TEST(test_daily_reset_adopts_without_resetting_first);
    RUN_TEST(test_daily_reset_fires_once_at_or_after_the_time);
    RUN_TEST(test_daily_reset_survives_a_restart_over_the_minute);
    RUN_TEST(test_daily_reset_handles_dst_and_clock_steps);
    RUN_TEST(test_reset_day_crosses_years);
    return UNITY_END();
}
