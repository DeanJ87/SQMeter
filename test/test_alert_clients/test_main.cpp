#include <unity.h>

#include "AlertEngine.h"

#include <ArduinoJson.h>

#include <cstring>
#include <fstream>
#include <sstream>

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
    // Back and silent again inside the cooldown: no more notifications.
    int sent = 0;
    for (uint32_t t = 140; t < 400; t += 10)
        sent += engine.update(clientInputs(t, true, (t / 20) % 2 == 0), rules).size();
    TEST_ASSERT_EQUAL(0, sent);
    // Back for good: sent once the cooldown has run.
    TEST_ASSERT_EQUAL(0, engine.update(clientInputs(420, true, false), rules).size());
    TEST_ASSERT_TRUE(hasType(engine.update(clientInputs(431, true, false), rules), AlertType::ClientBack));
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

namespace
{
    // lib/AlertLogic/template-variables.json (specs/008 FR-006).
    void assertFillsSharedVars(const Alert *alert, AlertType type)
    {
        const char *event = alertTypeName(type);
        TEST_ASSERT_NOT_NULL_MESSAGE(alert, event);
        std::ifstream in("lib/AlertLogic/template-variables.json");
        std::stringstream text;
        text << in.rdbuf();
        DynamicJsonDocument doc(4096);
        TEST_ASSERT_FALSE(deserializeJson(doc, text.str()));
        for (JsonVariantConst name : doc["events"][event].as<JsonArrayConst>())
        {
            bool filled = false;
            for (const auto &v : alert->vars)
                filled = filled || v.first == name.as<std::string>();
            TEST_ASSERT_TRUE_MESSAGE(filled, (std::string(event) + " does not fill {" + name.as<std::string>() + "}").c_str());
        }
    }
} // namespace

// Every per-event variable the UI offers is one the device fills.
void test_client_events_fill_the_shared_variables(void)
{
    AlertEngine engine;
    const AlertRules rules = clientRules();
    engine.update(clientInputs(0, true, false), rules);
    std::vector<Alert> alerts = engine.update(clientInputs(130, true, true), rules);
    assertFillsSharedVars(find(alerts, AlertType::ClientLost), AlertType::ClientLost);
    alerts = engine.update(clientInputs(400, true, false), rules);
    assertFillsSharedVars(find(alerts, AlertType::ClientBack), AlertType::ClientBack);
    alerts = engine.update(clientInputs(410, false, false, true), rules);
    assertFillsSharedVars(find(alerts, AlertType::ClientDisconnected), AlertType::ClientDisconnected);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_client_lost_and_back);
    RUN_TEST(test_client_disconnect_is_not_lost);
    RUN_TEST(test_client_disconnect_after_lost_sends_no_back);
    RUN_TEST(test_client_never_watched_never_alerts);
    RUN_TEST(test_client_flapping_respects_cooldown);
    RUN_TEST(test_client_events_ignore_grace_and_darkness);
    RUN_TEST(test_client_events_off);
    RUN_TEST(test_client_default_wording_names_no_product);
    RUN_TEST(test_client_events_fill_the_shared_variables);
    return UNITY_END();
}
