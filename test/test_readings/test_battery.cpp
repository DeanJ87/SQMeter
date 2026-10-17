#include <unity.h>

#include "Readings.h"

#include <map>
#include <string>

// The battery in the readings document (specs/032-battery, contracts/readings.md).

using namespace SQM::Readings;

namespace
{
    DynamicJsonDocument renderBattery(const Battery &b, const Groups &groups = Groups{})
    {
        Snapshot s;
        s.battery = b;
        DynamicJsonDocument doc(DOC_CAPACITY);
        write(doc.to<JsonObject>(), s, groups);
        return doc;
    }

    Battery discharging()
    {
        Battery b;
        b.present = true;
        b.status = Status::Ok;
        b.ageMs = 1200;
        b.chargerPins = true;
        b.percent = 82.43;
        b.voltage = 3.91234;
        b.ratePercentPerHour = -6.23;
        b.state = "discharging";
        b.usbPower = false;
        b.timeToEmptyMinutes = 797;
        return b;
    }
} // namespace

void test_battery_absent_is_present_false_only()
{
    DynamicJsonDocument doc = renderBattery(Battery{});
    JsonObject battery = doc["battery"];
    TEST_ASSERT_FALSE(battery.isNull());
    TEST_ASSERT_EQUAL(1, battery.size());
    TEST_ASSERT_FALSE(battery["present"].as<bool>());
}

void test_battery_full_object()
{
    DynamicJsonDocument doc = renderBattery(discharging());
    JsonObject b = doc["battery"];
    TEST_ASSERT_TRUE(b["present"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("ok", b["status"]);
    TEST_ASSERT_EQUAL(1200, b["ageMs"].as<int>());
    TEST_ASSERT_FALSE(b["estimating"].as<bool>());
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 82.4, b["percent"].as<double>());
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 3.912, b["voltage"].as<double>());
    TEST_ASSERT_FLOAT_WITHIN(1e-9, -6.2, b["ratePercentPerHour"].as<double>());
    TEST_ASSERT_EQUAL_STRING("discharging", b["state"]);
    TEST_ASSERT_FALSE(b["usbPower"].as<bool>());
    TEST_ASSERT_TRUE(b["usbPower"].is<bool>());
    TEST_ASSERT_TRUE(b["fault"].isNull());
    TEST_ASSERT_TRUE(b.containsKey("fault"));
    TEST_ASSERT_EQUAL(797, b["timeToEmptyMinutes"].as<int>());
    TEST_ASSERT_TRUE(b.containsKey("timeToFullMinutes"));
    TEST_ASSERT_TRUE(b["timeToFullMinutes"].isNull());
    TEST_ASSERT_FALSE(b["powerSaving"].as<bool>());
    TEST_ASSERT_EQUAL(13, b.size());
}

void test_battery_nulls_while_estimating_and_without_charger()
{
    Battery b = discharging();
    b.estimating = true;
    b.percent.reset();
    b.ratePercentPerHour.reset();
    b.timeToEmptyMinutes.reset();
    b.usbPower.reset();
    b.chargerPins = false;
    b.state = nullptr;
    DynamicJsonDocument doc = renderBattery(b);
    JsonObject out = doc["battery"];
    TEST_ASSERT_TRUE(out["estimating"].as<bool>());
    TEST_ASSERT_TRUE(out.containsKey("percent"));
    TEST_ASSERT_TRUE(out["percent"].isNull());
    TEST_ASSERT_TRUE(out["ratePercentPerHour"].isNull());
    TEST_ASSERT_TRUE(out["usbPower"].isNull());
    TEST_ASSERT_TRUE(out["state"].isNull());
    TEST_ASSERT_TRUE(out["timeToEmptyMinutes"].isNull());
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 3.912, out["voltage"].as<double>());
}

void test_battery_not_ok_carries_status_and_charger_only()
{
    Battery b = discharging();
    b.status = Status::Missing;
    b.state = "no-battery";
    b.usbPower = true;
    DynamicJsonDocument doc = renderBattery(b);
    JsonObject out = doc["battery"];
    TEST_ASSERT_EQUAL_STRING("missing", out["status"]);
    TEST_ASSERT_FALSE(out.containsKey("ageMs"));
    TEST_ASSERT_EQUAL_STRING("no-battery", out["state"]);
    TEST_ASSERT_TRUE(out["usbPower"].as<bool>());
    TEST_ASSERT_FALSE(out.containsKey("percent"));
    TEST_ASSERT_FALSE(out.containsKey("voltage"));
    TEST_ASSERT_EQUAL(4, out.size());

    // Without the charger pins: status only.
    b.chargerPins = false;
    b.status = Status::Stale;
    doc = renderBattery(b);
    out = doc["battery"];
    TEST_ASSERT_EQUAL_STRING("stale", out["status"]);
    TEST_ASSERT_EQUAL(1200, out["ageMs"].as<int>());
    TEST_ASSERT_EQUAL(3, out.size());
}

void test_battery_group_off_for_mqtt()
{
    Groups groups;
    groups.battery = false;
    DynamicJsonDocument doc = renderBattery(discharging(), groups);
    TEST_ASSERT_FALSE(doc.containsKey("battery"));
}

namespace
{
    std::map<std::string, std::string> batteryDiscovery(bool entities, bool group = true)
    {
        DiscoveryDevice device{"sqmeter_aabbccddeeff", "Roof SQM", "0.4.0", "sqmeter", "homeassistant"};
        Groups groups;
        groups.battery = group;
        groups.batteryEntities = entities;
        std::map<std::string, std::string> out;
        forEachDiscovery(device, groups, true, [&](const std::string &topic, const std::string &payload) { out[topic] = payload; });
        return out;
    }
    const std::string BATTERY_TOPIC = "homeassistant/sensor/sqmeter_aabbccddeeff/";
} // namespace

void test_battery_home_assistant_entities()
{
    auto on = batteryDiscovery(true);
    struct
    {
        const char *object;
        const char *name;
        const char *deviceClass;
        const char *unit;
        const char *value;
    } const expected[] = {
        {"battery", "Battery", "battery", "%", "value_json.battery.percent"},
        {"battery_voltage", "Battery voltage", "voltage", "V", "value_json.battery.voltage"},
        {"battery_state", "Charging state", "enum", nullptr, "value_json.battery.state"},
        {"battery_time_to_empty", "Battery time to empty", "duration", "min", "value_json.battery.timeToEmptyMinutes"},
    };
    for (const auto &e : expected)
    {
        const std::string payload = on[BATTERY_TOPIC + e.object + "/config"];
        TEST_ASSERT_FALSE_MESSAGE(payload.empty(), e.object);
        DynamicJsonDocument doc(2048);
        TEST_ASSERT_FALSE(deserializeJson(doc, payload));
        TEST_ASSERT_EQUAL_STRING(e.name, doc["name"]);
        TEST_ASSERT_EQUAL_STRING(e.deviceClass, doc["device_class"]);
        if (e.unit)
            TEST_ASSERT_EQUAL_STRING(e.unit, doc["unit_of_measurement"]);
        else
            TEST_ASSERT_FALSE(doc.containsKey("unit_of_measurement"));
        const std::string value = std::string("{{ ") + e.value + " if " + e.value + " is defined else none }}";
        TEST_ASSERT_EQUAL_STRING(value.c_str(), doc["value_template"]);
        TEST_ASSERT_EQUAL_STRING(
            "{{ 'online' if value_json.battery.status == 'ok' else 'offline' }}", doc["availability"][1]["value_template"]);
    }
    DynamicJsonDocument state(2048);
    deserializeJson(state, on[BATTERY_TOPIC + "battery_state/config"]);
    TEST_ASSERT_EQUAL(5, state["options"].size());
    TEST_ASSERT_EQUAL_STRING("no-battery", state["options"][4]);
    TEST_ASSERT_FALSE(state.containsKey("state_class"));

    // No gauge (or the group off): each removed with an empty payload.
    for (auto removed : {batteryDiscovery(false), batteryDiscovery(true, false)})
        for (const char *object : {"battery", "battery_voltage", "battery_state", "battery_time_to_empty"})
        {
            const std::string topic = BATTERY_TOPIC + object + "/config";
            TEST_ASSERT_TRUE_MESSAGE(removed.count(topic) == 1, object);
            TEST_ASSERT_TRUE_MESSAGE(removed[topic].empty(), object);
        }
}
