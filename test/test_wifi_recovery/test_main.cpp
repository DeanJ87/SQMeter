#include <unity.h>

#include "CaptivePortal.h"
#include "WifiRecovery.h"

#include <vector>

using namespace SQM;
using Action = WifiRecovery::Action;

void setUp() {}
void tearDown() {}

namespace
{
    constexpr uint32_t SECOND = 1000;
    constexpr uint32_t MINUTE = 60 * SECOND;

    // A device with saved credentials, driven a second at a time like the
    // main loop; carries out the hotspot actions the way WiFiManager does.
    struct Device
    {
        WifiRecovery recovery{1000, 300000};
        WifiRecovery::State state;
        std::vector<std::pair<uint32_t, Action>> actions;

        Device()
        {
            state.hasCredentials = true;
            recovery.begin(0);
        }

        Action step()
        {
            const Action action = recovery.next(state);
            if (action == Action::OpenHotspot)
                state.hotspotOpen = true;
            if (action == Action::CloseHotspot)
                state.hotspotOpen = false;
            if (action != Action::None)
                actions.emplace_back(state.now, action);
            return action;
        }

        // Runs until `until`, a second at a time.
        void runTo(uint32_t until)
        {
            while (state.now < until)
            {
                state.now += SECOND;
                step();
            }
        }

        size_t count(Action wanted, uint32_t from = 0) const
        {
            size_t n = 0;
            for (const auto &entry : actions)
                if (entry.second == wanted && entry.first >= from)
                    ++n;
            return n;
        }

        bool firstAt(Action wanted, uint32_t &at, uint32_t from = 0) const
        {
            for (const auto &entry : actions)
                if (entry.second == wanted && entry.first >= from)
                {
                    at = entry.first;
                    return true;
                }
            return false;
        }

        // Longest gap between station attempts (retry, reset, close) from `from`.
        uint32_t longestAttemptGap(uint32_t from, uint32_t to) const
        {
            uint32_t last = from;
            uint32_t longest = 0;
            for (const auto &entry : actions)
            {
                if (entry.first < from || entry.first > to)
                    continue;
                if (entry.second == Action::Retry || entry.second == Action::ResetStack || entry.second == Action::CloseHotspot)
                {
                    longest = std::max(longest, entry.first - last);
                    last = entry.first;
                }
            }
            return std::max(longest, to - last);
        }

        void joinedFor(uint32_t ms)
        {
            state.connected = true;
            runTo(state.now + ms);
            state.connected = false;
        }
    };
} // namespace

void test_no_credentials_opens_hotspot_at_once()
{
    WifiRecovery recovery(1000, 300000);
    recovery.begin(0);
    WifiRecovery::State state;
    TEST_ASSERT_EQUAL(static_cast<int>(Action::OpenHotspot), static_cast<int>(recovery.next(state)));
    state.hotspotOpen = true;
    state.now = WifiRecovery::RESTART_AFTER_MS * 2;
    TEST_ASSERT_EQUAL(static_cast<int>(Action::None), static_cast<int>(recovery.next(state)));
}

void test_connected_does_nothing()
{
    Device device;
    device.state.connected = true;
    device.runTo(WifiRecovery::RESTART_AFTER_MS * 2);
    TEST_ASSERT_TRUE(device.actions.empty());
    TEST_ASSERT_TRUE(device.recovery.joinedSinceBoot());
}

void test_boot_without_network_opens_hotspot_after_fallback()
{
    Device device;
    device.runTo(CaptivePortal::FALLBACK_AFTER_MS + 5 * SECOND);
    uint32_t at = 0;
    TEST_ASSERT_TRUE(device.firstAt(Action::OpenHotspot, at));
    TEST_ASSERT_EQUAL_UINT32(CaptivePortal::FALLBACK_AFTER_MS, at);
}

void test_outage_after_joining_never_opens_hotspot()
{
    Device device;
    device.joinedFor(10 * MINUTE);
    device.runTo(device.state.now + 14 * MINUTE);
    TEST_ASSERT_EQUAL(0, device.count(Action::OpenHotspot));
}

void test_backoff_is_capped_at_a_minute()
{
    // The configured maximum (5 min) is too slow for a safety monitor.
    Device device;
    device.joinedFor(MINUTE);
    const uint32_t from = device.state.now;
    device.runTo(from + 14 * MINUTE);
    TEST_ASSERT_TRUE(device.longestAttemptGap(from, device.state.now) <= WifiRecovery::RETRY_CAP_MS);
    TEST_ASSERT_EQUAL_UINT32(WifiRecovery::RETRY_CAP_MS, device.recovery.retryDelayMs());
}

void test_first_retries_are_quick()
{
    Device device;
    device.joinedFor(MINUTE);
    const uint32_t from = device.state.now;
    device.runTo(from + 10 * SECOND);
    TEST_ASSERT_TRUE(device.count(Action::Retry, from) >= 3); // 1 s, 2 s, 4 s
}

void test_stack_is_reset_every_few_minutes()
{
    Device device;
    device.joinedFor(MINUTE);
    const uint32_t from = device.state.now;
    device.runTo(from + 14 * MINUTE);
    uint32_t at = 0;
    TEST_ASSERT_TRUE(device.firstAt(Action::ResetStack, at, from));
    TEST_ASSERT_EQUAL_UINT32(from + SECOND + WifiRecovery::STACK_RESET_EVERY_MS, at); // down from the next loop
    TEST_ASSERT_EQUAL(4, device.count(Action::ResetStack, from));                     // 3, 6, 9, 12 min
}

void test_restarts_after_fifteen_minutes_off_the_network()
{
    Device device;
    device.joinedFor(MINUTE);
    const uint32_t from = device.state.now;
    device.runTo(from + 20 * MINUTE);
    uint32_t at = 0;
    TEST_ASSERT_TRUE(device.firstAt(Action::Restart, at));
    TEST_ASSERT_EQUAL_UINT32(from + SECOND + WifiRecovery::RESTART_AFTER_MS, at);
    TEST_ASSERT_EQUAL(1, device.count(Action::Restart)); // asked once
}

void test_rejoining_resets_the_restart_timer()
{
    Device device;
    device.joinedFor(MINUTE);
    device.runTo(device.state.now + 10 * MINUTE);
    device.joinedFor(SECOND);
    const uint32_t from = device.state.now;
    device.runTo(from + 10 * MINUTE);
    TEST_ASSERT_EQUAL(0, device.count(Action::Restart));
    device.runTo(from + 16 * MINUTE);
    TEST_ASSERT_EQUAL(1, device.count(Action::Restart));
}

void test_ap_restart_outage_recovers_without_restart()
{
    // The field case: joined, the access point goes for 5 minutes. Retries
    // and a stack reset carry on through it; no hotspot, no restart.
    Device device;
    device.joinedFor(30 * MINUTE);
    const uint32_t from = device.state.now;
    device.runTo(from + 5 * MINUTE);
    TEST_ASSERT_TRUE(device.count(Action::ResetStack, from) >= 1);
    device.joinedFor(MINUTE);
    TEST_ASSERT_EQUAL(0, device.count(Action::OpenHotspot));
    TEST_ASSERT_EQUAL(0, device.count(Action::Restart));
}

void test_unused_hotspot_closes_and_reopens_later()
{
    // Restarted during the outage (brown-out): not joined since boot, so the
    // hotspot opens - but nobody uses it, so it closes again.
    Device device;
    device.runTo(CaptivePortal::FALLBACK_AFTER_MS);
    TEST_ASSERT_TRUE(device.state.hotspotOpen);
    const uint32_t opened = device.state.now;
    device.runTo(opened + WifiRecovery::HOTSPOT_IDLE_CLOSE_MS + SECOND);
    uint32_t closed = 0;
    TEST_ASSERT_TRUE(device.firstAt(Action::CloseHotspot, closed));
    TEST_ASSERT_EQUAL_UINT32(opened + WifiRecovery::HOTSPOT_IDLE_CLOSE_MS, closed);
    TEST_ASSERT_FALSE(device.state.hotspotOpen);

    // Station alone for a while, then the hotspot is offered again.
    device.runTo(closed + WifiRecovery::HOTSPOT_REOPEN_AFTER_MS + SECOND);
    uint32_t reopened = 0;
    TEST_ASSERT_TRUE(device.firstAt(Action::OpenHotspot, reopened, closed));
    TEST_ASSERT_EQUAL_UINT32(closed + WifiRecovery::HOTSPOT_REOPEN_AFTER_MS, reopened);
}

void test_hotspot_retries_at_least_every_minute()
{
    Device device;
    device.runTo(CaptivePortal::FALLBACK_AFTER_MS);
    const uint32_t opened = device.state.now;
    device.runTo(opened + WifiRecovery::HOTSPOT_IDLE_CLOSE_MS - SECOND);
    TEST_ASSERT_TRUE(device.count(Action::Retry, opened) >= 3);
    TEST_ASSERT_TRUE(device.longestAttemptGap(opened, device.state.now) <= WifiRecovery::RETRY_CAP_MS);
    TEST_ASSERT_EQUAL(0, device.count(Action::ResetStack)); // never under the hotspot
}

void test_hotspot_in_use_stays_open_and_blocks_restart()
{
    Device device;
    device.runTo(CaptivePortal::FALLBACK_AFTER_MS);
    device.state.hotspotInUse = true;
    device.runTo(30 * MINUTE);
    TEST_ASSERT_EQUAL(0, device.count(Action::CloseHotspot));
    TEST_ASSERT_EQUAL(0, device.count(Action::Restart));

    // The phone leaves: the restart waits out the grace period.
    device.state.hotspotInUse = false;
    const uint32_t left = device.state.now;
    device.runTo(left + WifiRecovery::IN_USE_GRACE_MS - SECOND);
    TEST_ASSERT_EQUAL(0, device.count(Action::Restart));
    device.runTo(left + WifiRecovery::IN_USE_GRACE_MS);
    TEST_ASSERT_EQUAL(1, device.count(Action::Restart));
}

void test_busy_pauses_retries_and_restart()
{
    // Setup screen trying a network, or a firmware update running.
    Device device;
    device.joinedFor(MINUTE);
    device.state.busy = true;
    const uint32_t from = device.state.now;
    device.runTo(from + 20 * MINUTE);
    TEST_ASSERT_EQUAL(0, device.count(Action::Retry, from));
    TEST_ASSERT_EQUAL(0, device.count(Action::ResetStack, from));
    TEST_ASSERT_EQUAL(0, device.count(Action::Restart, from));
}

void test_device_needing_setup_still_gets_the_hotspot()
{
    // Wrong password: the hotspot keeps coming back within a few minutes,
    // and the restart only happens while nobody is on it.
    Device device;
    device.runTo(14 * MINUTE);
    TEST_ASSERT_TRUE(device.count(Action::OpenHotspot) >= 2);
    uint32_t restart = 0;
    device.runTo(16 * MINUTE);
    TEST_ASSERT_TRUE(device.firstAt(Action::Restart, restart));
    TEST_ASSERT_EQUAL_UINT32(WifiRecovery::RESTART_AFTER_MS + SECOND, restart); // down since the first loop
}

void test_auto_reconnect_off_leaves_station_alone()
{
    Device device;
    device.state.autoReconnect = false;
    device.joinedFor(MINUTE);
    const uint32_t from = device.state.now;
    device.runTo(from + 20 * MINUTE);
    TEST_ASSERT_EQUAL(0, device.count(Action::Retry, from));
    TEST_ASSERT_EQUAL(0, device.count(Action::ResetStack, from));
    TEST_ASSERT_EQUAL(0, device.count(Action::Restart, from));
}

void test_short_configured_maximum_is_kept()
{
    WifiRecovery recovery(1000, 8000);
    recovery.begin(0);
    WifiRecovery::State state;
    state.hasCredentials = true;
    state.connected = true;
    recovery.next(state);
    state.connected = false;
    for (state.now = 1; state.now < 60 * SECOND; ++state.now)
        recovery.next(state);
    TEST_ASSERT_EQUAL_UINT32(8000, recovery.retryDelayMs());
}

void test_new_credentials_restart_backoff()
{
    Device device;
    device.joinedFor(MINUTE);
    device.runTo(device.state.now + 5 * MINUTE);
    device.recovery.credentialsChanged(device.state.now);
    TEST_ASSERT_EQUAL_UINT32(1000, device.recovery.retryDelayMs());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_credentials_opens_hotspot_at_once);
    RUN_TEST(test_connected_does_nothing);
    RUN_TEST(test_boot_without_network_opens_hotspot_after_fallback);
    RUN_TEST(test_outage_after_joining_never_opens_hotspot);
    RUN_TEST(test_backoff_is_capped_at_a_minute);
    RUN_TEST(test_first_retries_are_quick);
    RUN_TEST(test_stack_is_reset_every_few_minutes);
    RUN_TEST(test_restarts_after_fifteen_minutes_off_the_network);
    RUN_TEST(test_rejoining_resets_the_restart_timer);
    RUN_TEST(test_ap_restart_outage_recovers_without_restart);
    RUN_TEST(test_unused_hotspot_closes_and_reopens_later);
    RUN_TEST(test_hotspot_retries_at_least_every_minute);
    RUN_TEST(test_hotspot_in_use_stays_open_and_blocks_restart);
    RUN_TEST(test_busy_pauses_retries_and_restart);
    RUN_TEST(test_device_needing_setup_still_gets_the_hotspot);
    RUN_TEST(test_auto_reconnect_off_leaves_station_alone);
    RUN_TEST(test_short_configured_maximum_is_kept);
    RUN_TEST(test_new_credentials_restart_backoff);
    return UNITY_END();
}
