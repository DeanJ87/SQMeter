#include <unity.h>

#include "AlertEngine.h"
#include "ClientWatch.h"

#include <cstring>

using namespace SQM::Alerts;

// The imaging-app events (specs/021): an Alpaca client that stops checking a
// device, comes back, or disconnects.

void setUp(void) {}
void tearDown(void) {}

namespace
{
    AlertRules noGraceRules()
    {
        AlertRules rules;
        rules.startupGraceSeconds = 0;
        rules.cooldownSeconds = 60;
        rules.sensorSettleSeconds = 0;
        rules.skySettleSeconds = 0;
        rules.skyNightOnly = false;
        return rules;
    }

    AlertInputs safeInputs(uint32_t now)
    {
        AlertInputs in;
        in.nowSeconds = now;
        in.safetyKnown = true;
        in.isSafe = true;
        return in;
    }

    bool hasType(const std::vector<Alert> &alerts, AlertType type)
    {
        for (const Alert &a : alerts)
            if (a.type == type)
                return true;
        return false;
    }
} // namespace

namespace
{
    AlertInputs clientInputs(uint32_t now, bool watching, bool silent, bool disconnectedNow = false)
    {
        AlertInputs in = safeInputs(now);
        in.clients[0].device = "safety monitor";
        in.clients[0].watching = watching;
        in.clients[0].silent = silent;
        in.clients[0].disconnectedNow = disconnectedNow;
        in.clients[0].silentFor = "2 min";
        in.clients[0].lastChecked = "21:04";
        in.clients[1].device = "weather device";
        return in;
    }

    AlertRules clientRules()
    {
        AlertRules rules = noGraceRules();
        rules.onClientLost = true;
        rules.onClientBack = true;
        rules.onClientDisconnected = true;
        return rules;
    }

    const Alert *find(const std::vector<Alert> &alerts, AlertType type)
    {
        for (const Alert &a : alerts)
            if (a.type == type)
                return &a;
        return nullptr;
    }

    std::string var(const Alert &alert, const char *name)
    {
        for (const auto &v : alert.vars)
            if (v.first == name)
                return v.second;
        return "";
    }
} // namespace

void test_client_lost_and_back(void)
{
    AlertEngine engine;
    const AlertRules rules = clientRules();
    engine.update(clientInputs(0, true, false), rules);
    std::vector<Alert> alerts = engine.update(clientInputs(130, true, true), rules);
    const Alert *lost = find(alerts, AlertType::ClientLost);
    TEST_ASSERT_NOT_NULL(lost);
    TEST_ASSERT_EQUAL_STRING("Imaging app stopped checking", lost->title.c_str());
    TEST_ASSERT_EQUAL_STRING("No request to the safety monitor for 2 min - last checked 21:04.", lost->message.c_str());
    TEST_ASSERT_EQUAL_STRING("safety monitor", var(*lost, "device").c_str());
    TEST_ASSERT_EQUAL_STRING("2 min", var(*lost, "silent_for").c_str());
    // Once per loss.
    TEST_ASSERT_FALSE(hasType(engine.update(clientInputs(140, true, true), rules), AlertType::ClientLost));
    alerts = engine.update(clientInputs(200, true, false), rules);
    TEST_ASSERT_NOT_NULL(find(alerts, AlertType::ClientBack));
    TEST_ASSERT_EQUAL_STRING("The safety monitor is being checked again.", find(alerts, AlertType::ClientBack)->message.c_str());
}

void test_client_disconnect_is_not_lost(void)
{
    AlertEngine engine;
    const AlertRules rules = clientRules();
    engine.update(clientInputs(0, true, false), rules);
    std::vector<Alert> alerts = engine.update(clientInputs(10, false, false, true), rules);
    TEST_ASSERT_NOT_NULL(find(alerts, AlertType::ClientDisconnected));
    TEST_ASSERT_EQUAL_STRING("The safety monitor was disconnected.", find(alerts, AlertType::ClientDisconnected)->message.c_str());
    for (uint32_t t = 20; t < 2000; t += 10)
        TEST_ASSERT_EQUAL(0, engine.update(clientInputs(t, false, false), rules).size());
}

void test_client_disconnect_after_lost_sends_no_back(void)
{
    AlertEngine engine;
    const AlertRules rules = clientRules();
    engine.update(clientInputs(0, true, false), rules);
    TEST_ASSERT_TRUE(hasType(engine.update(clientInputs(130, true, true), rules), AlertType::ClientLost));
    std::vector<Alert> alerts = engine.update(clientInputs(400, false, false, true), rules);
    TEST_ASSERT_FALSE(hasType(alerts, AlertType::ClientBack));
    TEST_ASSERT_TRUE(hasType(alerts, AlertType::ClientDisconnected));
}

void test_client_never_watched_never_alerts(void)
{
    AlertEngine engine;
    const AlertRules rules = clientRules();
    for (uint32_t t = 0; t < 2000; t += 10)
        TEST_ASSERT_EQUAL(0, engine.update(clientInputs(t, false, false), rules).size());
}

void test_client_flapping_respects_cooldown(void)
{
    AlertEngine engine;
    AlertRules rules = clientRules();
    rules.cooldownSeconds = 300;
    engine.update(clientInputs(0, true, false), rules);
    TEST_ASSERT_TRUE(hasType(engine.update(clientInputs(130, true, true), rules), AlertType::ClientLost));
    // A quick return answers the loss straight away, not after the cooldown.
    TEST_ASSERT_TRUE(hasType(engine.update(clientInputs(140, true, false), rules), AlertType::ClientBack));
    // Flapping inside the cooldown: no more notifications.
    int sent = 0;
    for (uint32_t t = 150; t < 420; t += 10)
        sent += engine.update(clientInputs(t, true, (t / 20) % 2 == 0), rules).size();
    TEST_ASSERT_EQUAL(0, sent);
    // Still silent once the cooldown has run: one more loss, then its one "back".
    TEST_ASSERT_TRUE(hasType(engine.update(clientInputs(440, true, true), rules), AlertType::ClientLost));
    std::vector<Alert> alerts = engine.update(clientInputs(450, true, false), rules);
    TEST_ASSERT_EQUAL(1, alerts.size());
    TEST_ASSERT_TRUE(hasType(alerts, AlertType::ClientBack));
    TEST_ASSERT_EQUAL(0, engine.update(clientInputs(460, true, false), rules).size());
}

void test_client_back_only_answers_a_sent_loss(void)
{
    AlertEngine engine;
    AlertRules rules = clientRules();
    engine.update(clientInputs(0, true, false), rules);
    // Silent while not watched (alerts paused): no loss, so no "back" either.
    engine.update(clientInputs(130, false, true), rules);
    TEST_ASSERT_FALSE(hasType(engine.update(clientInputs(200, true, false), rules), AlertType::ClientBack));
}

void test_client_events_ignore_grace_and_darkness(void)
{
    AlertEngine engine;
    AlertRules rules = clientRules();
    rules.startupGraceSeconds = 600;
    rules.safetyNightOnly = true;
    rules.skyNightOnly = true;
    AlertInputs in = clientInputs(0, true, false);
    in.nightKnown = true;
    in.isNight = false;
    engine.update(in, rules);
    in = clientInputs(130, true, true);
    in.nightKnown = true;
    in.isNight = false;
    TEST_ASSERT_TRUE(hasType(engine.update(in, rules), AlertType::ClientLost));
}

void test_client_events_off(void)
{
    AlertEngine engine;
    AlertRules rules = noGraceRules();
    rules.onClientLost = false;
    rules.onClientBack = false;
    rules.onClientDisconnected = false;
    engine.update(clientInputs(0, true, false), rules);
    TEST_ASSERT_EQUAL(0, engine.update(clientInputs(130, true, true), rules).size());
    TEST_ASSERT_EQUAL(0, engine.update(clientInputs(200, false, false, true), rules).size());
}

void test_client_default_wording_names_no_product(void)
{
    AlertEngine engine;
    const AlertRules rules = clientRules();
    engine.update(clientInputs(0, true, false), rules);
    std::vector<Alert> alerts = engine.update(clientInputs(130, true, true), rules);
    std::vector<Alert> more = engine.update(clientInputs(500, true, false), rules);
    alerts.insert(alerts.end(), more.begin(), more.end());
    more = engine.update(clientInputs(900, false, false, true), rules);
    alerts.insert(alerts.end(), more.begin(), more.end());
    TEST_ASSERT_EQUAL(3, alerts.size());
    for (const Alert &a : alerts)
    {
        TEST_ASSERT_NULL(strstr(a.title.c_str(), "N.I.N.A"));
        TEST_ASSERT_NULL(strstr(a.message.c_str(), "N.I.N.A"));
        TEST_ASSERT_NULL(strstr(a.message.c_str(), "NINA"));
    }
    TEST_ASSERT_EQUAL_STRING("client_lost", alertTypeName(AlertType::ClientLost));
    TEST_ASSERT_EQUAL_STRING("client_back", alertTypeName(AlertType::ClientBack));
    TEST_ASSERT_EQUAL_STRING("client_disconnected", alertTypeName(AlertType::ClientDisconnected));
}

// --- Connections kept across a self-caused restart (#136) ---

namespace
{
    namespace Alpaca = SQM::Alpaca;

    // The device side: ClientWatch on the Router's counters, feeding the
    // engine the safety monitor's state the way the firmware does.
    struct Restarted
    {
        Alpaca::DeviceActivity activity[Alpaca::DEVICE_COUNT];
        uint32_t silence[Alpaca::DEVICE_COUNT] = {120000, 600000};
        Alpaca::ClientWatch watch;
        AlertEngine engine;
        AlertRules rules = clientRules();

        explicit Restarted(bool restored)
        {
            if (restored)
            {
                activity[0].connected = true;
                const bool connected[Alpaca::DEVICE_COUNT] = {true, false};
                watch.restoreConnections(connected, 0);
            }
        }

        std::vector<Alert> at(uint32_t seconds)
        {
            watch.update(activity, silence, true, seconds * 1000);
            const Alpaca::ClientState &sm = watch.state(Alpaca::Device::SafetyMonitor);
            return engine.update(clientInputs(seconds, sm.watching, sm.silent, sm.disconnectedNow), rules);
        }
    };
} // namespace

void test_restored_connection_silent_after_restart_is_lost(void)
{
    Restarted device(true);
    TEST_ASSERT_TRUE(device.watch.state(Alpaca::Device::SafetyMonitor).everRequested);
    for (uint32_t t = 0; t <= 120; t += 10)
        TEST_ASSERT_FALSE(hasType(device.at(t), AlertType::ClientLost));
    TEST_ASSERT_TRUE(hasType(device.at(130), AlertType::ClientLost));
    // The app comes back: one request answers the loss.
    device.activity[0].requests++;
    TEST_ASSERT_TRUE(hasType(device.at(140), AlertType::ClientBack));
    TEST_ASSERT_FALSE(device.watch.state(Alpaca::Device::SafetyMonitor).silent);
}

void test_restored_connection_still_polled_is_not_lost(void)
{
    Restarted device(true);
    for (uint32_t t = 0; t < 600; t += 10)
    {
        device.activity[0].requests++;
        TEST_ASSERT_EQUAL(0, device.at(t).size());
    }
}

void test_normal_boot_without_restore_is_unchanged(void)
{
    Restarted device(false);
    const Alpaca::ClientState &sm = device.watch.state(Alpaca::Device::SafetyMonitor);
    for (uint32_t t = 0; t < 600; t += 10)
        TEST_ASSERT_EQUAL(0, device.at(t).size());
    TEST_ASSERT_FALSE(sm.watching);
    TEST_ASSERT_FALSE(sm.everRequested);
    // Only a request after boot starts the clock, as before.
    device.activity[0].requests++;
    device.at(600);
    TEST_ASSERT_TRUE(sm.watching);
    TEST_ASSERT_FALSE(hasType(device.at(700), AlertType::ClientLost));
    TEST_ASSERT_TRUE(hasType(device.at(730), AlertType::ClientLost));
    // The restore only touches the devices that were connected.
    TEST_ASSERT_FALSE(device.watch.state(Alpaca::Device::ObservingConditions).everRequested);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_client_lost_and_back);
    RUN_TEST(test_client_disconnect_is_not_lost);
    RUN_TEST(test_client_disconnect_after_lost_sends_no_back);
    RUN_TEST(test_client_never_watched_never_alerts);
    RUN_TEST(test_client_flapping_respects_cooldown);
    RUN_TEST(test_client_back_only_answers_a_sent_loss);
    RUN_TEST(test_client_events_ignore_grace_and_darkness);
    RUN_TEST(test_client_events_off);
    RUN_TEST(test_client_default_wording_names_no_product);
    RUN_TEST(test_restored_connection_silent_after_restart_is_lost);
    RUN_TEST(test_restored_connection_still_polled_is_not_lost);
    RUN_TEST(test_normal_boot_without_restore_is_unchanged);
    return UNITY_END();
}
