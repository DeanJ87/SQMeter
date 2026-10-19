#include <unity.h>

#include "BatteryLogic.h"

// The MCP73871's status lines (specs/032-battery research D3).

using namespace SQM::Battery;

namespace
{
    // Levels as written in the truth table: 'L' or 'H'.
    ChargerPins pins(char stat1, char stat2, char pg)
    {
        return {stat1 == 'H', stat2 == 'H', pg == 'H'};
    }

    struct Row
    {
        const char *levels; // STAT1, STAT2, PG as "LHL"
        ChargeState state;
        bool usb;
        ChargeFault fault = ChargeFault::None;
    };

    void expectRow(const Row &row)
    {
        const ChargeState state = row.state;
        const bool usb = row.usb;
        const ChargeFault fault = row.fault;
        const ChargerOutput out = decodeCharger(pins(row.levels[0], row.levels[1], row.levels[2]));
        TEST_ASSERT_EQUAL(static_cast<int>(state), static_cast<int>(out.state));
        TEST_ASSERT_TRUE(out.usbPower.has_value());
        TEST_ASSERT_EQUAL(usb, *out.usbPower);
        TEST_ASSERT_EQUAL(static_cast<int>(fault), static_cast<int>(out.fault));
    }
} // namespace

void test_truth_table(void)
{
    expectRow({"LHL", ChargeState::Charging, true});
    expectRow({"HLL", ChargeState::Charged, true});
    expectRow({"LLL", ChargeState::Fault, true, ChargeFault::Temperature});
    expectRow({"HHL", ChargeState::NoBattery, true});
    expectRow({"HHH", ChargeState::Discharging, false});
    expectRow({"LHH", ChargeState::Discharging, false}); // the charger's low-battery output
    // Unexpected patterns: from PG.
    expectRow({"HLH", ChargeState::Discharging, false});
    expectRow({"LLH", ChargeState::Discharging, false});
}

void test_pattern_must_hold_two_seconds(void)
{
    ChargerDecoder d;
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Unknown), static_cast<int>(d.update(pins('H', 'H', 'H'), 0).state));
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Unknown), static_cast<int>(d.update(pins('H', 'H', 'H'), 1999).state));
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Discharging), static_cast<int>(d.update(pins('H', 'H', 'H'), 2000).state));

    // USB plugged in: still discharging until charging has held 2 s.
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Discharging), static_cast<int>(d.update(pins('L', 'H', 'L'), 10000).state));
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Discharging), static_cast<int>(d.update(pins('L', 'H', 'L'), 11000).state));
    const ChargerOutput out = d.update(pins('L', 'H', 'L'), 12000);
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Charging), static_cast<int>(out.state));
    TEST_ASSERT_TRUE(*out.usbPower);
}

void test_change_reported_within_five_seconds(void)
{
    // SC-002: sampled every second (the loop polls the lines), a change is
    // reported within 5 s.
    ChargerDecoder d;
    for (uint32_t t = 0; t <= 5000; t += 1000)
        d.update(pins('L', 'H', 'L'), t);
    uint32_t reportedAt = 0;
    for (uint32_t t = 6000; t <= 12000 && reportedAt == 0; t += 1000)
        if (d.update(pins('H', 'L', 'L'), t).state == ChargeState::Charged)
            reportedAt = t;
    TEST_ASSERT_NOT_EQUAL(0, reportedAt);
    TEST_ASSERT_LESS_OR_EQUAL(6000 + 5000, reportedAt);
}

void test_toggling_reads_as_no_battery(void)
{
    ChargerDecoder d;
    for (uint32_t t = 0; t <= 3000; t += 500)
        d.update(pins('H', 'H', 'H'), t);
    // Pack pulled out on USB: STAT lines flicker.
    ChargerOutput out;
    bool flip = false;
    uint32_t t = 4000;
    for (; t <= 9500; t += 500, flip = !flip)
    {
        out = d.update(flip ? pins('L', 'H', 'L') : pins('H', 'L', 'L'), t);
        TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Discharging), static_cast<int>(out.state));
    }
    out = d.update(flip ? pins('L', 'H', 'L') : pins('H', 'L', 'L'), 10500);
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::NoBattery), static_cast<int>(out.state));
    TEST_ASSERT_TRUE(*out.usbPower);

    // The lines settle: the settled pattern is reported.
    d.update(pins('H', 'H', 'L'), 11000);
    out = d.update(pins('H', 'H', 'L'), 13000);
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::NoBattery), static_cast<int>(out.state));
    out = d.update(pins('L', 'H', 'L'), 14000);
    out = d.update(pins('L', 'H', 'L'), 16000);
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Charging), static_cast<int>(out.state));
}

void test_without_charger_pins_from_rate(void)
{
    ChargerOutput out = chargerFromRate(false, 5.0f);
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Unknown), static_cast<int>(out.state));
    TEST_ASSERT_FALSE(out.usbPower.has_value());
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Charging), static_cast<int>(chargerFromRate(true, 0.6f).state));
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Discharging), static_cast<int>(chargerFromRate(true, 0.5f).state));
    TEST_ASSERT_EQUAL(static_cast<int>(ChargeState::Discharging), static_cast<int>(chargerFromRate(true, -3.0f).state));
    TEST_ASSERT_FALSE(chargerFromRate(true, 3.0f).usbPower.has_value());
}

void run_charger_tests(void)
{
    RUN_TEST(test_truth_table);
    RUN_TEST(test_pattern_must_hold_two_seconds);
    RUN_TEST(test_change_reported_within_five_seconds);
    RUN_TEST(test_toggling_reads_as_no_battery);
    RUN_TEST(test_without_charger_pins_from_rate);
}
