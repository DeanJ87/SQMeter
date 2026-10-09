#include <unity.h>

#include "AlertEngine.h"

#include <ArduinoJson.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace SQM::Alerts;

// specs/008 FR-006 / SC-003: every per-event {variable} the web UI offers
// (lib/AlertLogic/template-variables.json) is one the device fills. The
// common variables are checked in test_device_core (Core::alertVars), the UI
// side in web/src/components/__tests__/alertVariables.test.ts.

void setUp(void) {}
void tearDown(void) {}

namespace
{
    AlertRules quickRules()
    {
        AlertRules rules;
        rules.startupGraceSeconds = 0;
        rules.cooldownSeconds = 0;
        rules.sensorSettleSeconds = 0;
        rules.skySettleSeconds = 0;
        rules.skyNightOnly = false;
        rules.onDewRisk = true;
        rules.dewRiskMarginC = 2.0f;
        rules.onClientLost = true;
        rules.onClientBack = true;
        rules.onClientDisconnected = true;
        return rules;
    }

    AlertInputs calm(uint32_t now)
    {
        AlertInputs in;
        in.nowSeconds = now;
        in.safetyKnown = true;
        in.isSafe = true;
        in.rainEnabled = true;
        for (size_t i = 0; i < SENSOR_COUNT; ++i)
        {
            in.sensors[i].name = "Sensor";
            in.sensors[i].enabled = i < 4;
            in.sensors[i].healthy = true;
        }
        in.environmentValid = true;
        in.temperatureC = 10.0f;
        in.dewpointC = 0.0f;
        in.clients[0].device = "safety monitor";
        in.clients[1].device = "weather device";
        return in;
    }

    std::vector<std::string> sharedEventVars(const char *event)
    {
        std::ifstream in("lib/AlertLogic/template-variables.json");
        std::stringstream text;
        text << in.rdbuf();
        DynamicJsonDocument doc(4096);
        TEST_ASSERT_FALSE(deserializeJson(doc, text.str()));
        std::vector<std::string> names;
        for (JsonVariantConst name : doc["events"][event].as<JsonArrayConst>())
            names.push_back(name.as<std::string>());
        return names;
    }

    void assertFillsSharedVars(const std::vector<Alert> &alerts, AlertType type)
    {
        const char *event = alertTypeName(type);
        const Alert *alert = nullptr;
        for (const Alert &a : alerts)
            if (a.type == type)
                alert = &a;
        TEST_ASSERT_NOT_NULL_MESSAGE(alert, event);
        for (const std::string &name : sharedEventVars(event))
        {
            bool filled = false;
            for (const auto &v : alert->vars)
                filled = filled || v.first == name;
            TEST_ASSERT_TRUE_MESSAGE(filled, (std::string(event) + " does not fill {" + name + "}").c_str());
        }
    }
} // namespace

void test_safety_and_sensor_events_fill_their_variables(void)
{
    const AlertRules rules = quickRules();
    AlertEngine engine;
    engine.update(calm(0), rules);

    AlertInputs bad = calm(10);
    bad.isSafe = false;
    bad.unsafeReasons = {"Cloud 62% >= 35%"};
    bad.sensors[2].name = "BME280";
    bad.sensors[2].healthy = false;
    bad.lensFault = true;
    bad.dewpointC = 8.5f;
    const std::vector<Alert> alerts = engine.update(bad, rules);
    assertFillsSharedVars(alerts, AlertType::Unsafe);
    assertFillsSharedVars(alerts, AlertType::SensorFault);
    assertFillsSharedVars(alerts, AlertType::LensFault);
    assertFillsSharedVars(alerts, AlertType::DewRisk);

    assertFillsSharedVars(engine.update(calm(100), rules), AlertType::SensorRecovered);
}

void test_client_events_fill_their_variables(void)
{
    const AlertRules rules = quickRules();
    AlertEngine engine;
    auto client = [](uint32_t now, bool watching, bool silent, bool disconnectedNow)
    {
        AlertInputs in = calm(now);
        in.clients[0].watching = watching;
        in.clients[0].silent = silent;
        in.clients[0].disconnectedNow = disconnectedNow;
        in.clients[0].silentFor = "2 min";
        in.clients[0].lastChecked = "21:04";
        return in;
    };
    engine.update(client(0, true, false, false), rules);
    assertFillsSharedVars(engine.update(client(130, true, true, false), rules), AlertType::ClientLost);
    assertFillsSharedVars(engine.update(client(400, true, false, false), rules), AlertType::ClientBack);
    assertFillsSharedVars(engine.update(client(410, false, false, true), rules), AlertType::ClientDisconnected);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_safety_and_sensor_events_fill_their_variables);
    RUN_TEST(test_client_events_fill_their_variables);
    return UNITY_END();
}
