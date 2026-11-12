#include <unity.h>

#include <ArduinoJson.h>

#include "WebHealthJson.h"
#include "WebStallMonitor.h"

#include <cstdio>

// The web server stall detector and TCP counts behind /api/status
// connections.tcp and connections.stalls.

using namespace SQM::WebHealth;

void setUp(void) {}
void tearDown(void) {}

namespace
{
    constexpr uint8_t ESTABLISHED = static_cast<uint8_t>(TcpState::Established);
    constexpr uint8_t FIN_WAIT_2 = static_cast<uint8_t>(TcpState::FinWait2);
    constexpr uint8_t TIME_WAIT = static_cast<uint8_t>(TcpState::TimeWait);
    constexpr uint8_t LISTEN = static_cast<uint8_t>(TcpState::Listen);

    TcpCounts counts(uint8_t established, uint8_t finWait2, uint8_t timeWait)
    {
        TcpCounts c;
        c.add(LISTEN);
        for (uint8_t i = 0; i < established; ++i)
            c.add(ESTABLISHED);
        for (uint8_t i = 0; i < finWait2; ++i)
            c.add(FIN_WAIT_2);
        for (uint8_t i = 0; i < timeWait; ++i)
            c.add(TIME_WAIT);
        return c;
    }

    // An imaging app polling every 2 s, sampled once a second.
    struct Device
    {
        WebStallMonitor monitor;
        Sample sample;

        Device()
        {
            sample.nowMs = 10000;
            sample.epoch = 1791417600;
            sample.freeHeap = 200000;
            sample.asyncTcpStackFree = 900;
            sample.asyncTcp = TaskActivity::Idle;
            sample.tcp = counts(1, 0, 3);
        }

        void tick(bool served)
        {
            sample.nowMs += 1000;
            sample.epoch += 1;
            if (served)
            {
                sample.accepted += 6;
                sample.alpacaRequests += 6;
            }
            monitor.update(sample);
        }

        void run(int seconds, bool served)
        {
            for (int i = 0; i < seconds; ++i)
                tick(served);
        }
    };
} // namespace

void test_total_counts_pool_pcbs_but_not_listeners(void)
{
    TcpCounts c = counts(2, 3, 4);
    TEST_ASSERT_EQUAL(9, c.total());
    TEST_ASSERT_EQUAL(1, c.count(TcpState::Listen));
    c.add(42); // not a state lwIP has: ignored
    TEST_ASSERT_EQUAL(9, c.total());
    for (int i = 0; i < 300; ++i)
        c.add(TIME_WAIT);
    TEST_ASSERT_EQUAL(255, c.count(TcpState::TimeWait));
}

void test_state_names_are_camel_case(void)
{
    TEST_ASSERT_EQUAL_STRING("finWait2", stateName(TcpState::FinWait2));
    TEST_ASSERT_EQUAL_STRING("timeWait", stateName(TcpState::TimeWait));
    TEST_ASSERT_EQUAL_STRING("synRcvd", stateName(TcpState::SynRcvd));
}

void test_peak_keeps_the_first_highest_sample(void)
{
    Device d;
    d.run(3, true);
    d.sample.tcp = counts(2, 6, 8);
    d.tick(true);
    const uint32_t peakAt = d.sample.nowMs;
    d.sample.tcp = counts(2, 8, 6); // as many: not a new peak
    d.tick(true);
    d.sample.tcp = counts(1, 0, 2);
    d.tick(true);
    TEST_ASSERT_EQUAL(16, d.monitor.peak().tcp.total());
    TEST_ASSERT_EQUAL(6, d.monitor.peak().tcp.count(TcpState::FinWait2));
    TEST_ASSERT_EQUAL(peakAt, d.monitor.peak().uptimeMs);
}

void test_a_gap_under_five_seconds_is_not_a_stall(void)
{
    Device d;
    d.run(10, true);
    d.run(4, false);
    TEST_ASSERT_FALSE(d.monitor.stalled());
    d.run(1, true);
    TEST_ASSERT_EQUAL(0, d.monitor.stallCount());
}

void test_a_gap_of_five_seconds_while_polled_is_a_stall(void)
{
    Device d;
    d.run(10, true);
    const uint32_t lastServed = d.sample.nowMs;
    d.sample.tcp = counts(0, 9, 6);
    d.run(5, false);
    TEST_ASSERT_TRUE(d.monitor.stalled());
    TEST_ASSERT_EQUAL(lastServed, d.monitor.current().startUptimeMs);
    TEST_ASSERT_EQUAL(9, d.monitor.current().atDetection.count(TcpState::FinWait2));
}

void test_a_stall_records_duration_worst_and_minimums_when_it_ends(void)
{
    Device d;
    d.run(10, true);
    const uint32_t lastServed = d.sample.nowMs;
    const uint32_t lastServedEpoch = d.sample.epoch;
    d.sample.tcp = counts(0, 9, 6);
    d.run(6, false);
    d.sample.tcp = counts(1, 12, 3);
    d.sample.freeHeap = 150000;
    d.sample.asyncTcp = TaskActivity::Busy;
    d.run(2, false);
    d.sample.tcp = counts(0, 4, 2);
    d.sample.freeHeap = 180000;
    d.sample.asyncTcp = TaskActivity::Idle;
    d.run(12, false);
    d.run(1, true);

    TEST_ASSERT_FALSE(d.monitor.stalled());
    TEST_ASSERT_EQUAL(1, d.monitor.stallCount());
    const Stall &s = d.monitor.stall(0);
    TEST_ASSERT_TRUE(s.resumed);
    TEST_ASSERT_EQUAL(21000, s.durationMs);
    TEST_ASSERT_EQUAL(lastServed, s.startUptimeMs);
    TEST_ASSERT_EQUAL(lastServedEpoch, s.startEpoch);
    TEST_ASSERT_EQUAL(15, s.atDetection.total());
    TEST_ASSERT_EQUAL(16, s.worst.total());
    TEST_ASSERT_EQUAL(12, s.worst.count(TcpState::FinWait2));
    TEST_ASSERT_EQUAL(150000, s.minFreeHeap);
    TEST_ASSERT_EQUAL(16, s.samples); // from detection (5 s) to the last silent second
    TEST_ASSERT_EQUAL(14, s.asyncTcpIdle);
}

void test_no_stall_without_an_imaging_app_polling(void)
{
    Device d;
    d.sample.accepted = 1;
    d.monitor.update(d.sample);
    d.run(20, false); // a quiet web UI: no new connections, no Alpaca requests
    TEST_ASSERT_FALSE(d.monitor.stalled());

    d.run(5, true);
    // The app stopped; only the web UI connected for the next minute.
    for (int i = 0; i < 31; ++i)
    {
        d.sample.accepted += 1;
        d.run(2, false);
    }
    d.run(10, false);
    TEST_ASSERT_FALSE(d.monitor.stalled());
    TEST_ASSERT_EQUAL(0, d.monitor.stallCount());
}

void test_a_stall_past_a_minute_is_closed_unresumed_once(void)
{
    Device d;
    d.run(10, true);
    d.run(70, false);
    TEST_ASSERT_FALSE(d.monitor.stalled());
    TEST_ASSERT_EQUAL(1, d.monitor.stallCount());
    TEST_ASSERT_FALSE(d.monitor.stall(0).resumed);
    TEST_ASSERT_EQUAL(WebStallMonitor::STALL_GIVE_UP_MS, d.monitor.stall(0).durationMs);
    d.run(120, false);
    TEST_ASSERT_EQUAL(1, d.monitor.stallCount());
    d.run(10, true);
    d.run(6, false);
    TEST_ASSERT_TRUE(d.monitor.stalled());
}

void test_history_keeps_the_newest_eight_newest_first(void)
{
    Device d;
    d.run(10, true);
    for (int i = 0; i < 10; ++i)
    {
        d.run(5 + i, false);
        d.run(3, true);
    }
    TEST_ASSERT_EQUAL(10, d.monitor.stallCount());
    TEST_ASSERT_EQUAL(WebStallMonitor::STALL_HISTORY, d.monitor.storedStalls());
    TEST_ASSERT_EQUAL(15000, d.monitor.stall(0).durationMs); // gap of 14 s, ended on the 15th
    TEST_ASSERT_EQUAL(8000, d.monitor.stall(7).durationMs);
}

void test_stall_detection_survives_the_millis_wrap(void)
{
    Device d;
    d.sample.nowMs = 0xFFFFF000UL;
    d.run(3, true);
    d.run(6, false);
    TEST_ASSERT_TRUE(d.monitor.stalled());
    d.run(1, true);
    TEST_ASSERT_EQUAL(7000, d.monitor.stall(0).durationMs);
}

void test_json_leaves_out_empty_states(void)
{
    DynamicJsonDocument doc(1024);
    writeTcpCounts(doc.to<JsonObject>(), counts(2, 0, 5));
    TEST_ASSERT_EQUAL(1, doc["listen"].as<int>());
    TEST_ASSERT_EQUAL(2, doc["established"].as<int>());
    TEST_ASSERT_EQUAL(5, doc["timeWait"].as<int>());
    TEST_ASSERT_FALSE(doc.containsKey("finWait2"));
    TEST_ASSERT_EQUAL(7, doc["total"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["listenBacklog"].as<int>());
}

namespace
{
    size_t slots(JsonVariantConst value)
    {
        size_t count = 0;
        if (value.is<JsonObjectConst>())
            for (JsonPairConst member : value.as<JsonObjectConst>())
                count += 1 + slots(member.value());
        else if (value.is<JsonArrayConst>())
            for (JsonVariantConst element : value.as<JsonArrayConst>())
                count += 1 + slots(element);
        return count;
    }

    // ArduinoJson's slot is 16 bytes on the ESP32 and bigger on the host
    // (as test_status_document).
    constexpr size_t DEVICE_SLOT_BYTES = 16;
    size_t onDevice(const JsonDocument &doc)
    {
        const size_t hostSlot = JSON_OBJECT_SIZE(1);
        return doc.memoryUsage() - slots(doc.as<JsonVariantConst>()) * (hostSlot - DEVICE_SLOT_BYTES);
    }

    TcpCounts everyState()
    {
        TcpCounts c;
        for (uint8_t state = 0; state < TCP_STATE_COUNT; ++state)
            c.add(state);
        c.listenBacklog = 5;
        return c;
    }
} // namespace

void test_documents_fit_their_pool(void)
{
    Device d;
    d.sample.tcp = everyState();
    d.run(10, true);
    for (size_t i = 0; i < WebStallMonitor::STALL_HISTORY + 2; ++i)
    {
        d.run(70, false);
        d.run(3, true);
    }
    TEST_ASSERT_EQUAL(WebStallMonitor::STALL_HISTORY, d.monitor.storedStalls());

    DynamicJsonDocument doc(DOC_CAPACITY * 2);
    writeTcp(doc.createNestedObject("tcp"), everyState(), 4294967295U, d.monitor.peak());
    writeStalls(doc.createNestedObject("stalls"), d.monitor);
    TEST_ASSERT_FALSE(doc.overflowed());
    TEST_ASSERT_EQUAL(11, doc["stalls"]["recent"][0]["worst"].size() - 2);

    const size_t used = onDevice(doc);
    char detail[96];
    snprintf(detail, sizeof detail, "pool %u bytes of %u", static_cast<unsigned>(used), static_cast<unsigned>(DOC_CAPACITY));
    TEST_ASSERT_TRUE_MESSAGE(used * 12 / 10 <= DOC_CAPACITY, detail);
    printf("web health sections on the device: %s\n", detail);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_total_counts_pool_pcbs_but_not_listeners);
    RUN_TEST(test_state_names_are_camel_case);
    RUN_TEST(test_peak_keeps_the_first_highest_sample);
    RUN_TEST(test_a_gap_under_five_seconds_is_not_a_stall);
    RUN_TEST(test_a_gap_of_five_seconds_while_polled_is_a_stall);
    RUN_TEST(test_a_stall_records_duration_worst_and_minimums_when_it_ends);
    RUN_TEST(test_no_stall_without_an_imaging_app_polling);
    RUN_TEST(test_a_stall_past_a_minute_is_closed_unresumed_once);
    RUN_TEST(test_history_keeps_the_newest_eight_newest_first);
    RUN_TEST(test_stall_detection_survives_the_millis_wrap);
    RUN_TEST(test_json_leaves_out_empty_states);
    RUN_TEST(test_documents_fit_their_pool);
    return UNITY_END();
}
