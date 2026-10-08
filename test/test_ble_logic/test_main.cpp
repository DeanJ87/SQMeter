#include <unity.h>
#include "BlePayloads.h"
#include "BleAlarm.h"

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

// --- Alarm / ack / heartbeat ---

void test_alarm_raise_encode(void)
{
    AlarmTracker alarm;
    TEST_ASSERT_TRUE(alarm.raiseAlarm(1u << 9, 1759500000u, 1000));
    TEST_ASSERT_TRUE(alarm.active());
    std::string p = alarm.encode();
    TEST_ASSERT_EQUAL(13, p.size());
    TEST_ASSERT_EQUAL_UINT8(1, byteAt(p, 0));            // seq 1
    TEST_ASSERT_EQUAL_UINT8(ALARM_ACTIVE, byteAt(p, 4));
    TEST_ASSERT_EQUAL_UINT8(0x02, byteAt(p, 6));        // reason bit 9 -> byte 1 of flags
}

void test_alarm_repeats_until_acknowledged(void)
{
    AlarmTracker alarm;
    alarm.raiseAlarm(1, 0, 0);
    TEST_ASSERT_FALSE(alarm.resendDue(29999));
    TEST_ASSERT_TRUE(alarm.resendDue(30000));
    alarm.markSent(30000);
    TEST_ASSERT_FALSE(alarm.resendDue(45000));

    TEST_ASSERT_TRUE(alarm.acknowledge(1, 46000));
    TEST_ASSERT_FALSE(alarm.active());
    TEST_ASSERT_FALSE(alarm.resendDue(200000));
    TEST_ASSERT_EQUAL_UINT8(ALARM_NONE, byteAt(alarm.encode(), 4));
}

void test_stale_ack_cannot_cancel_newer_alarm(void)
{
    AlarmTracker alarm;
    alarm.raiseAlarm(1, 0, 0);
    alarm.acknowledge(1, 10);
    alarm.raiseAlarm(2, 0, 20); // seq 2
    TEST_ASSERT_FALSE(alarm.acknowledge(1, 30));
    TEST_ASSERT_TRUE(alarm.active());
    TEST_ASSERT_TRUE(alarm.acknowledge(2, 40));
}

void test_repeat_raise_keeps_sequence(void)
{
    AlarmTracker alarm;
    alarm.raiseAlarm(1, 0, 0);
    TEST_ASSERT_FALSE(alarm.raiseAlarm(4, 0, 10)); // e.g. rain then unsafe
    TEST_ASSERT_EQUAL_UINT32(1, alarm.sequence());
    TEST_ASSERT_EQUAL_UINT8(5, byteAt(alarm.encode(), 5));
}

void test_info_never_overrides_active_alarm(void)
{
    AlarmTracker alarm;
    TEST_ASSERT_TRUE(alarm.raiseInfo(0, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(ALARM_INFO, byteAt(alarm.encode(), 4));
    alarm.raiseAlarm(1, 0, 10);
    TEST_ASSERT_FALSE(alarm.raiseInfo(0, 0, 20)); // "safe again" must not silence the phone
    TEST_ASSERT_TRUE(alarm.active());
    TEST_ASSERT_FALSE(alarm.acknowledge(1, 30)); // info took seq 1; the alarm is seq 2
    TEST_ASSERT_TRUE(alarm.acknowledge(2, 30));
}

void test_heartbeat_and_ack_decoding(void)
{
    std::string hb = encodeHeartbeat(3, 3600);
    TEST_ASSERT_EQUAL(8, hb.size());
    TEST_ASSERT_EQUAL_UINT8(3, byteAt(hb, 0));
    TEST_ASSERT_EQUAL_UINT8(0x10, byteAt(hb, 4)); // 3600 = 0x0E10
    TEST_ASSERT_EQUAL_UINT8(0x0E, byteAt(hb, 5));

    const uint8_t ack[] = {0x02, 0x01, 0x00, 0x00};
    uint32_t seq = 0;
    TEST_ASSERT_TRUE(decodeAck(ack, 4, seq));
    TEST_ASSERT_EQUAL_UINT32(258, seq);
    TEST_ASSERT_FALSE(decodeAck(ack, 3, seq));
    TEST_ASSERT_FALSE(decodeAck(nullptr, 4, seq));
}

void test_passkey_parsing(void)
{
    uint32_t key = 0;
    TEST_ASSERT_TRUE(parsePasskey("482913", key));
    TEST_ASSERT_EQUAL_UINT32(482913, key);
    TEST_ASSERT_TRUE(parsePasskey("012345", key));
    TEST_ASSERT_EQUAL_UINT32(12345, key);
    TEST_ASSERT_FALSE(parsePasskey("000000", key));
    TEST_ASSERT_FALSE(parsePasskey("12345", key));
    TEST_ASSERT_FALSE(parsePasskey("12a456", key));
    TEST_ASSERT_FALSE(parsePasskey("", key));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_safety_payload);
    RUN_TEST(test_unknown_safety_never_reports_safe);
    RUN_TEST(test_rain_payload);
    RUN_TEST(test_advertisement);
    RUN_TEST(test_alarm_raise_encode);
    RUN_TEST(test_alarm_repeats_until_acknowledged);
    RUN_TEST(test_stale_ack_cannot_cancel_newer_alarm);
    RUN_TEST(test_repeat_raise_keeps_sequence);
    RUN_TEST(test_info_never_overrides_active_alarm);
    RUN_TEST(test_heartbeat_and_ack_decoding);
    RUN_TEST(test_passkey_parsing);
    return UNITY_END();
}
