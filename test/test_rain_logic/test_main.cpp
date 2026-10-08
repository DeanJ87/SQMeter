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
    return UNITY_END();
}
