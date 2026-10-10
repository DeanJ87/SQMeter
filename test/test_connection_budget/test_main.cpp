#include <unity.h>

#include "AlpacaRouter.h"
#include "ConnectionBudget.h"
#include "ConnectionMemory.h"

#include <string>

// Imaging apps keep their connection (spec 011 FR-008, spec 013 FR-012): the
// live-update sockets leave room for them, and a restart the device caused
// itself doesn't drop them.

using SQM::Alpaca::ConnectionMemory;
using SQM::Alpaca::ResetKind;
namespace Budget = SQM::ConnectionBudget;

namespace
{
    class Backend : public SQM::Alpaca::Backend
    {
    public:
        bool alpacaEnabled() const override { return true; }
        bool isSafe() const override { return true; }
        SQM::Alpaca::ObservingConditionsSnapshot observingConditions() const override { return {}; }
        std::string serverName() const override { return "SQMeter"; }
        std::string location() const override { return ""; }
        std::string timestampUtc() const override { return ""; }
    };

    std::string get(SQM::Alpaca::Router &router, const std::string &path)
    {
        SQM::Alpaca::Request request;
        request.get = true;
        request.path = path;
        SQM::Alpaca::Response response;
        router.handle(request, response);
        return response.body;
    }

    void putConnected(SQM::Alpaca::Router &router, bool value)
    {
        SQM::Alpaca::Request request;
        request.put = true;
        request.path = "/api/v1/safetymonitor/0/connected";
        request.params = {{"Connected", value ? "True" : "False"}, {"ClientID", "7"}};
        SQM::Alpaca::Response response;
        router.handle(request, response);
    }
} // namespace

void test_live_updates_leave_room_for_imaging_apps()
{
    TEST_ASSERT_EQUAL(16, Budget::TCP_CONNECTIONS);
    TEST_ASSERT_TRUE(Budget::httpReserve() >= Budget::MIN_HTTP_RESERVE);
    TEST_ASSERT_EQUAL(8, Budget::httpReserve());
}

void test_oldest_client_is_replaced_past_the_limit()
{
    TEST_ASSERT_FALSE(Budget::overLimit(Budget::WEBSOCKETS_PER_ENDPOINT));
    TEST_ASSERT_TRUE(Budget::overLimit(Budget::WEBSOCKETS_PER_ENDPOINT + 1));
}

void test_a_brief_backlog_is_not_a_stall()
{
    TEST_ASSERT_FALSE(Budget::stalledTooLong(0, 50000));          // not full
    TEST_ASSERT_FALSE(Budget::stalledTooLong(1000, 1000 + 9999)); // full for under 10 s
    TEST_ASSERT_TRUE(Budget::stalledTooLong(1000, 1000 + 10000));
    TEST_ASSERT_TRUE(Budget::stalledTooLong(0xFFFFF000UL, 0x2000UL)); // across the millis() wrap
}

void test_only_restarts_the_device_caused_keep_connections()
{
    TEST_ASSERT_TRUE(SQM::Alpaca::keepsConnections(ResetKind::Software));
    TEST_ASSERT_TRUE(SQM::Alpaca::keepsConnections(ResetKind::Panic));
    TEST_ASSERT_TRUE(SQM::Alpaca::keepsConnections(ResetKind::Watchdog));
    TEST_ASSERT_FALSE(SQM::Alpaca::keepsConnections(ResetKind::PowerOn));
    TEST_ASSERT_FALSE(SQM::Alpaca::keepsConnections(ResetKind::Brownout));
    TEST_ASSERT_FALSE(SQM::Alpaca::keepsConnections(ResetKind::External));
    TEST_ASSERT_FALSE(SQM::Alpaca::keepsConnections(ResetKind::Unknown));
}

void test_memory_round_trips()
{
    const bool in[SQM::Alpaca::DEVICE_COUNT] = {true, false};
    bool out[SQM::Alpaca::DEVICE_COUNT] = {false, true};
    TEST_ASSERT_TRUE(SQM::Alpaca::recallConnections(SQM::Alpaca::rememberConnections(in), out));
    TEST_ASSERT_TRUE(out[0]);
    TEST_ASSERT_FALSE(out[1]);
}

void test_random_power_on_memory_is_ignored()
{
    bool out[SQM::Alpaca::DEVICE_COUNT] = {};
    ConnectionMemory garbage;
    garbage.magic = 0xDEADBEEF;
    garbage.connected = 3;
    garbage.check = 0;
    TEST_ASSERT_FALSE(SQM::Alpaca::recallConnections(garbage, out));

    const bool both[SQM::Alpaca::DEVICE_COUNT] = {true, true};
    ConnectionMemory tampered = SQM::Alpaca::rememberConnections(both);
    tampered.connected = 0x7; // a bit for a device that doesn't exist
    TEST_ASSERT_FALSE(SQM::Alpaca::recallConnections(tampered, out));
}

void test_restored_connection_reads_connected_without_a_disconnect()
{
    Backend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    const bool connected[SQM::Alpaca::DEVICE_COUNT] = {true, false};
    router.restoreConnections(connected);

    TEST_ASSERT_TRUE(router.anyConnected());
    TEST_ASSERT_NOT_EQUAL(std::string::npos, get(router, "/api/v1/safetymonitor/0/connected").find("\"Value\":true"));
    TEST_ASSERT_EQUAL(0, router.activity(SQM::Alpaca::Device::SafetyMonitor).disconnects);

    bool now[SQM::Alpaca::DEVICE_COUNT] = {};
    router.connectedDevices(now);
    TEST_ASSERT_TRUE(now[0]);
    TEST_ASSERT_FALSE(now[1]);
}

void test_a_clean_disconnect_is_remembered_as_disconnected()
{
    Backend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    putConnected(router, true);
    putConnected(router, false);
    bool now[SQM::Alpaca::DEVICE_COUNT] = {true, true};
    router.connectedDevices(now);
    bool recalled[SQM::Alpaca::DEVICE_COUNT] = {true, true};
    TEST_ASSERT_TRUE(SQM::Alpaca::recallConnections(SQM::Alpaca::rememberConnections(now), recalled));
    TEST_ASSERT_FALSE(recalled[0]);
    TEST_ASSERT_FALSE(recalled[1]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_live_updates_leave_room_for_imaging_apps);
    RUN_TEST(test_oldest_client_is_replaced_past_the_limit);
    RUN_TEST(test_a_brief_backlog_is_not_a_stall);
    RUN_TEST(test_only_restarts_the_device_caused_keep_connections);
    RUN_TEST(test_memory_round_trips);
    RUN_TEST(test_random_power_on_memory_is_ignored);
    RUN_TEST(test_restored_connection_reads_connected_without_a_disconnect);
    RUN_TEST(test_a_clean_disconnect_is_remembered_as_disconnected);
    return UNITY_END();
}
