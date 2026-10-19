#include <unity.h>

#include "BatteryLogic.h"

// MAX17048 decoding (specs/032-battery research D1, D2, D4).

using namespace SQM::Battery;

namespace
{
    GaugeSample sample(float volts, float percent, float ratePerHour = 0.0f, uint16_t status = 0)
    {
        GaugeSample s;
        s.vcell = static_cast<uint16_t>(volts / 78.125e-6f + 0.5f);
        s.soc = static_cast<uint16_t>(percent * 256.0f + 0.5f);
        s.crate = static_cast<int16_t>(ratePerHour / 0.208f + (ratePerHour < 0 ? -0.5f : 0.5f));
        s.status = status;
        return s;
    }
} // namespace

void test_register_scaling(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 3.9f, voltageFromVcell(49920)); // 49920 * 78.125 uV
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 4.2f, voltageFromVcell(53760));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 82.5f, percentFromSoc(82 * 256 + 128));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -6.24f, rateFromCrate(-30));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.4f, rateFromCrate(50));
}

void test_percent_clamped(void)
{
    // The gauge can read just above 100 % on a full pack.
    TEST_ASSERT_EQUAL_FLOAT(100.0f, percentFromSoc(101 * 256));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, percentFromSoc(0xFFFF));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, percentFromSoc(0));
}

void test_version_detection(void)
{
    TEST_ASSERT_TRUE(isMax17048(0x0010));
    TEST_ASSERT_TRUE(isMax17048(0x0012));
    TEST_ASSERT_TRUE(isMax17048(0x001F));
    TEST_ASSERT_FALSE(isMax17048(0x0000));
    TEST_ASSERT_FALSE(isMax17048(0x0020));
    TEST_ASSERT_FALSE(isMax17048(0xFFFF));
    TEST_ASSERT_TRUE(resetIndicated(0x0100));
    TEST_ASSERT_FALSE(resetIndicated(0x00FF));
}

void test_estimating_for_two_seconds_after_detection_and_reset(void)
{
    GaugeDecoder d;
    GaugeDecoder::Output o = d.update(sample(3.9f, 80.0f), 1000);
    TEST_ASSERT_TRUE(o.estimating);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 3.9f, o.voltage); // voltage is valid at once
    TEST_ASSERT_TRUE(d.update(sample(3.9f, 80.0f), 2999).estimating);
    TEST_ASSERT_FALSE(d.update(sample(3.9f, 80.0f), 3000).estimating);

    // The gauge reports a reset (RI): estimating again.
    o = d.update(sample(3.9f, 80.0f, 0.0f, STATUS_RESET_INDICATOR), 10000);
    TEST_ASSERT_TRUE(o.estimating);
    TEST_ASSERT_TRUE(o.resetSeen);
    TEST_ASSERT_TRUE(d.update(sample(3.9f, 80.0f), 11000).estimating);
    TEST_ASSERT_FALSE(d.update(sample(3.9f, 80.0f), 12000).estimating);

    // Found again after it went missing.
    d.restart();
    TEST_ASSERT_TRUE(d.update(sample(3.9f, 80.0f), 20000).estimating);
}

void test_zero_percent_above_3v5_is_estimating(void)
{
    GaugeDecoder d;
    d.update(sample(3.8f, 0.0f), 0);
    TEST_ASSERT_TRUE(d.update(sample(3.8f, 0.0f), 10000).estimating);
    // A genuinely empty pack.
    TEST_ASSERT_FALSE(d.update(sample(3.3f, 0.0f), 20000).estimating);
    TEST_ASSERT_FALSE(d.update(sample(3.8f, 1.0f), 30000).estimating);
}

void test_rate_average_valid_after_a_minute(void)
{
    GaugeDecoder d;
    uint32_t t = 0;
    d.update(sample(3.9f, 80.0f, -6.24f), t); // estimating
    GaugeDecoder::Output o;
    for (t = 2000; t < 62000; t += 5000)
    {
        o = d.update(sample(3.9f, 80.0f, -6.24f), t);
        TEST_ASSERT_FALSE(o.rateValid);
    }
    o = d.update(sample(3.9f, 80.0f, -6.24f), 62000);
    TEST_ASSERT_TRUE(o.rateValid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -6.24f, o.rate);

    // A step in the rate moves the 10-minute average about 63% in 10 minutes.
    for (t = 67000; t <= 662000; t += 5000)
        o = d.update(sample(3.9f, 80.0f, 0.0f), t);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, -6.24f * 0.368f, o.rate);
}

void test_rate_restarts_after_a_reset(void)
{
    GaugeDecoder d;
    for (uint32_t t = 0; t <= 70000; t += 5000)
        d.update(sample(3.9f, 80.0f, -5.0f), t);
    GaugeDecoder::Output o = d.update(sample(3.9f, 80.0f, -5.0f, STATUS_RESET_INDICATOR), 75000);
    TEST_ASSERT_FALSE(o.rateValid);
    o = d.update(sample(3.9f, 80.0f, -5.0f), 80000);
    TEST_ASSERT_FALSE(o.rateValid);
}

void run_gauge_tests(void)
{
    RUN_TEST(test_register_scaling);
    RUN_TEST(test_percent_clamped);
    RUN_TEST(test_version_detection);
    RUN_TEST(test_estimating_for_two_seconds_after_detection_and_reset);
    RUN_TEST(test_zero_percent_above_3v5_is_estimating);
    RUN_TEST(test_rate_average_valid_after_a_minute);
    RUN_TEST(test_rate_restarts_after_a_reset);
}
