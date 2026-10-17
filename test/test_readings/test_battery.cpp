#include <unity.h>

#include "Readings.h"

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
