#include <unity.h>

#include "GpsTime.h"

using namespace SQM;

// GPS week-number rollover: a NEO-6M on the main device reported
// 2007-03-23 00:34:49 at 2026-11-06 00:34:49 UTC.

void setUp() {}
void tearDown() {}

void test_utc_epoch()
{
    TEST_ASSERT_EQUAL_INT64(0, GpsTime::utcEpoch({1970, 1, 1, 0, 0, 0}));
    TEST_ASSERT_EQUAL_INT64(951782400, GpsTime::utcEpoch({2000, 2, 29, 0, 0, 0}));
    TEST_ASSERT_EQUAL_INT64(1793925289, GpsTime::utcEpoch({2026, 11, 6, 0, 34, 49}));
}

void test_rolled_back_date_is_moved_forward_1024_weeks()
{
    const int64_t reported = GpsTime::utcEpoch({2007, 3, 23, 0, 34, 49});
    TEST_ASSERT_EQUAL_INT64(GpsTime::utcEpoch({2026, 11, 6, 0, 34, 49}), GpsTime::undoRollover(reported));
}

void test_current_date_is_unchanged()
{
    const int64_t now = GpsTime::utcEpoch({2026, 11, 6, 0, 34, 49});
    TEST_ASSERT_EQUAL_INT64(now, GpsTime::undoRollover(now));
    TEST_ASSERT_EQUAL_INT64(GpsTime::EARLIEST_EPOCH, GpsTime::undoRollover(GpsTime::EARLIEST_EPOCH));
}

void test_two_rollovers()
{
    // A receiver two spans out (1987) still lands in the present span.
    const int64_t reported = GpsTime::utcEpoch({2026, 11, 6, 0, 34, 49}) - 2 * GpsTime::ROLLOVER_SECONDS;
    TEST_ASSERT_EQUAL_INT64(GpsTime::utcEpoch({2026, 11, 6, 0, 34, 49}), GpsTime::undoRollover(reported));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_utc_epoch);
    RUN_TEST(test_rolled_back_date_is_moved_forward_1024_weeks);
    RUN_TEST(test_current_date_is_unchanged);
    RUN_TEST(test_two_rollovers);
    return UNITY_END();
}
