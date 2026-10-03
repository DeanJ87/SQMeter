#include <unity.h>
#include "BlePayloads.h"

using namespace SQM::Ble;

void setUp(void) {}
void tearDown(void) {}

static uint8_t byteAt(const std::string &s, size_t i) { return static_cast<uint8_t>(s[i]); }

void test_safety_payload(void)
{
    State state;
    state.safetyKnown = true;
    state.isSafe = false;
    state.rawSafe = false;
    state.reasonFlags = 0x00000210; // cloud + rain
    std::string p = encodeSafety(state);
    TEST_ASSERT_EQUAL(6, p.size());
    TEST_ASSERT_EQUAL_UINT8(0, byteAt(p, 0));
    TEST_ASSERT_EQUAL_UINT8(0x10, byteAt(p, 2));
    TEST_ASSERT_EQUAL_UINT8(0x02, byteAt(p, 3));
    TEST_ASSERT_EQUAL_UINT8(0, byteAt(p, 5));
}

void test_unknown_safety_never_reports_safe(void)
{
    State state;
    state.isSafe = true; // but not yet evaluated
    TEST_ASSERT_EQUAL_UINT8(0, byteAt(encodeSafety(state), 0));
    TEST_ASSERT_EQUAL_UINT8(0, stateFlags(state) & FLAG_SAFE);
}

void test_rain_payload(void)
{
    State state;
    state.rainEnabled = true;
    state.rainHealthy = true;
    state.raining = true;
    state.rainRateMmPerHour = 2.47f;
    std::string p = encodeRain(state);
    TEST_ASSERT_EQUAL(3, p.size());
    TEST_ASSERT_EQUAL_UINT8(FLAG_RAIN_SENSOR | FLAG_RAINING | FLAG_RAIN_HEALTHY, byteAt(p, 0));
    TEST_ASSERT_EQUAL_UINT16(247, byteAt(p, 1) | (byteAt(p, 2) << 8));

    state.rainEnabled = false;
    p = encodeRain(state);
    TEST_ASSERT_EQUAL_UINT8(0, byteAt(p, 0));
    TEST_ASSERT_EQUAL_UINT16(0, byteAt(p, 1) | (byteAt(p, 2) << 8));
}

void test_advertisement(void)
{
    State state;
    state.safetyKnown = true;
    state.isSafe = true;
    state.sqmValid = true;
    state.sqm = 21.34f;
    std::string p = encodeAdvertisement(state);
    TEST_ASSERT_EQUAL(8, p.size());
    TEST_ASSERT_EQUAL_UINT8(0xFF, byteAt(p, 0));
    TEST_ASSERT_EQUAL_UINT8(0xFF, byteAt(p, 1));
    TEST_ASSERT_EQUAL_UINT8('S', byteAt(p, 2));
    TEST_ASSERT_EQUAL_UINT8('Q', byteAt(p, 3));
    TEST_ASSERT_EQUAL_UINT8(ADVERT_VERSION, byteAt(p, 4));
    TEST_ASSERT_EQUAL_UINT8(FLAG_SAFETY_KNOWN | FLAG_SAFE, byteAt(p, 5));
    TEST_ASSERT_EQUAL_UINT16(2134, byteAt(p, 6) | (byteAt(p, 7) << 8));

    state.sqmValid = false;
    p = encodeAdvertisement(state);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, byteAt(p, 6) | (byteAt(p, 7) << 8));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_safety_payload);
    RUN_TEST(test_unknown_safety_never_reports_safe);
    RUN_TEST(test_rain_payload);
    RUN_TEST(test_advertisement);
    return UNITY_END();
}
