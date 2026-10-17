#include <unity.h>

#include <ArduinoJson.h>

#include <cstdio>
#include <string>

#include "Config.h"

// Battery settings (specs/032-battery, contracts/config.md).

using namespace SQM;

namespace
{
    std::string batteryReject(const std::string &json, const Config &base)
    {
        std::string error;
        const auto cfg = Config::fromJson(json, &base, &error);
        return cfg ? std::string() : error;
    }

    std::string batteryReject(const std::string &json)
    {
        return batteryReject(json, Config::createDefault());
    }

    const char *const LOW_MSG = "Battery: low level must be 5 to 50%";
    const char *const CRITICAL_MSG = "Battery: critical level must be 2 to 25% and below the low level";
    const char *const SHUTDOWN_MSG = "Battery: shutdown level must be 2 to 10% and below the critical level";
    const char *const CAPACITY_MSG = "Battery: capacity must be 500 to 50000 mAh, or 0 if unknown";
    const char *const INTERVAL_MSG = "Battery: power-saving read interval must be 10 to 300 seconds";
    const char *const GRACE_MSG = "Battery: warning time before sleep must be 10 to 300 seconds";
    const char *const PINS_MSG = "Battery: pins must be valid and different";
    const char *const CHARGER_MSG = "Battery: set all three charger pins or none";
    const char *const USED_MSG = "Battery: pin is already in use";
    const char *const PG_MSG = "Battery: the USB power pin must be able to wake the device";
    const char *const LIMIT_MSG = "Safety: battery limit must be 2 to 50%";

    const char *const EVENT_KEYS[] = {"battery_low", "battery_critical", "battery_shutdown", "power_restored", "charge_complete", "charge_fault"};

    // A charger wired to 25/26/27 (27 is free with the anemometer off).
    const char *const CHARGER = "\"stat1Pin\":25,\"stat2Pin\":26,\"pgPin\":27";

    std::string battery(const std::string &body)
    {
        return "{\"battery\":{" + body + "}}";
    }
} // namespace

void test_battery_defaults()
{
    const Config c = Config::createDefault();
    const BatteryConfig &b = c.battery;
    TEST_ASSERT_TRUE(b.enabled);
    TEST_ASSERT_EQUAL_UINT8(20, b.lowPercent);
    TEST_ASSERT_EQUAL_UINT8(10, b.criticalPercent);
    TEST_ASSERT_EQUAL_UINT8(5, b.shutdownPercent);
    TEST_ASSERT_EQUAL_UINT16(0, b.capacityMah);
    TEST_ASSERT_TRUE(b.powerSaving);
    TEST_ASSERT_EQUAL_UINT16(30, b.saveReadIntervalSeconds);
    TEST_ASSERT_EQUAL_UINT16(30, b.shutdownGraceSeconds);
    TEST_ASSERT_EQUAL_UINT8(255, b.alertPin);
    TEST_ASSERT_EQUAL_UINT8(255, b.stat1Pin);
    TEST_ASSERT_EQUAL_UINT8(255, b.stat2Pin);
    TEST_ASSERT_EQUAL_UINT8(255, b.pgPin);
    TEST_ASSERT_FALSE(b.chargerPins());
    TEST_ASSERT_FALSE(c.alpaca.batteryEnabled);
    TEST_ASSERT_EQUAL_UINT8(10, c.alpaca.batteryMinPercent);
    TEST_ASSERT_TRUE(c.alpaca.batteryAlert);
    TEST_ASSERT_TRUE(c.alpaca.batteryPhoneAlarm);
    TEST_ASSERT_TRUE(c.mqtt.publish.battery);
    TEST_ASSERT_EQUAL_UINT8(2, c.alerts.batteryLow.level);
    TEST_ASSERT_EQUAL_UINT8(4, c.alerts.batteryCritical.level);
    TEST_ASSERT_EQUAL_UINT8(4, c.alerts.batteryShutdown.level);
    TEST_ASSERT_EQUAL_UINT8(0, c.alerts.powerRestored.level);
    TEST_ASSERT_EQUAL_UINT8(0, c.alerts.chargeComplete.level);
    TEST_ASSERT_EQUAL_UINT8(2, c.alerts.chargeFault.level);
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(c.validate(&error), error.c_str());

    DynamicJsonDocument doc(16384);
    TEST_ASSERT_FALSE(deserializeJson(doc, c.toJson()));
    JsonObject section = doc["battery"];
    TEST_ASSERT_FALSE(section.isNull());
    TEST_ASSERT_EQUAL(12, section.size());
    TEST_ASSERT_EQUAL(20, section["lowPercent"].as<int>());
    TEST_ASSERT_EQUAL(255, section["pgPin"].as<int>());
    TEST_ASSERT_FALSE(doc["alpaca"]["batteryEnabled"].as<bool>());
    TEST_ASSERT_EQUAL(10, doc["alpaca"]["batteryMinPercent"].as<int>());
    TEST_ASSERT_TRUE(doc["alpaca"]["batteryAlert"].as<bool>());
    TEST_ASSERT_TRUE(doc["alpaca"]["batteryPhoneAlarm"].as<bool>());
    TEST_ASSERT_TRUE(doc["mqtt"]["publish"]["battery"].as<bool>());
    for (const char *key : EVENT_KEYS)
        TEST_ASSERT_FALSE_MESSAGE(doc["alerts"]["events"][key].isNull(), key);
    TEST_ASSERT_EQUAL(4, doc["alerts"]["events"]["battery_critical"]["level"].as<int>());
}

void test_battery_json_round_trip()
{
    Config c = Config::createDefault();
    c.battery.enabled = false;
    c.battery.lowPercent = 30;
    c.battery.criticalPercent = 15;
    c.battery.shutdownPercent = 8;
    c.battery.capacityMah = 7000;
    c.battery.powerSaving = false;
    c.battery.saveReadIntervalSeconds = 120;
    c.battery.shutdownGraceSeconds = 60;
    c.battery.alertPin = 33;
    c.battery.stat1Pin = 25;
    c.battery.stat2Pin = 26;
    c.battery.pgPin = 32;
    c.alpaca.batteryEnabled = true;
    c.alpaca.batteryMinPercent = 25;
    c.alpaca.batteryAlert = false;
    c.alpaca.batteryPhoneAlarm = false;
    c.mqtt.publish.battery = false;
    c.alerts.chargeFault = {3, "siren", "Fault {battery}", "State {battery_state}"};

    std::string error;
    const auto loaded = Config::fromJson(c.toJson(), nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    const BatteryConfig &b = loaded->battery;
    TEST_ASSERT_FALSE(b.enabled);
    TEST_ASSERT_EQUAL_UINT8(30, b.lowPercent);
    TEST_ASSERT_EQUAL_UINT8(15, b.criticalPercent);
    TEST_ASSERT_EQUAL_UINT8(8, b.shutdownPercent);
    TEST_ASSERT_EQUAL_UINT16(7000, b.capacityMah);
    TEST_ASSERT_FALSE(b.powerSaving);
    TEST_ASSERT_EQUAL_UINT16(120, b.saveReadIntervalSeconds);
    TEST_ASSERT_EQUAL_UINT16(60, b.shutdownGraceSeconds);
    TEST_ASSERT_EQUAL_UINT8(33, b.alertPin);
    TEST_ASSERT_EQUAL_UINT8(25, b.stat1Pin);
    TEST_ASSERT_EQUAL_UINT8(26, b.stat2Pin);
    TEST_ASSERT_EQUAL_UINT8(32, b.pgPin);
    TEST_ASSERT_TRUE(b.chargerPins());
    TEST_ASSERT_TRUE(loaded->alpaca.batteryEnabled);
    TEST_ASSERT_EQUAL_UINT8(25, loaded->alpaca.batteryMinPercent);
    TEST_ASSERT_FALSE(loaded->alpaca.batteryAlert);
    TEST_ASSERT_FALSE(loaded->alpaca.batteryPhoneAlarm);
    TEST_ASSERT_FALSE(loaded->mqtt.publish.battery);
    TEST_ASSERT_EQUAL_UINT8(3, loaded->alerts.chargeFault.level);
    TEST_ASSERT_EQUAL_STRING("State {battery_state}", loaded->alerts.chargeFault.message.c_str());
}

void test_battery_level_validation()
{
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"lowPercent\":50")).c_str());
    TEST_ASSERT_EQUAL_STRING(LOW_MSG, batteryReject(battery("\"lowPercent\":51")).c_str());
    TEST_ASSERT_EQUAL_STRING(LOW_MSG, batteryReject(battery("\"lowPercent\":4")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"lowPercent\":11")).c_str());
    TEST_ASSERT_EQUAL_STRING(CRITICAL_MSG, batteryReject(battery("\"lowPercent\":10")).c_str());
    TEST_ASSERT_EQUAL_STRING(CRITICAL_MSG, batteryReject(battery("\"criticalPercent\":26,\"lowPercent\":40")).c_str());
    TEST_ASSERT_EQUAL_STRING(CRITICAL_MSG, batteryReject(battery("\"criticalPercent\":1")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"criticalPercent\":6")).c_str());
    TEST_ASSERT_EQUAL_STRING(SHUTDOWN_MSG, batteryReject(battery("\"criticalPercent\":5")).c_str());
    TEST_ASSERT_EQUAL_STRING(SHUTDOWN_MSG, batteryReject(battery("\"shutdownPercent\":11,\"criticalPercent\":20,\"lowPercent\":30")).c_str());
    TEST_ASSERT_EQUAL_STRING(SHUTDOWN_MSG, batteryReject(battery("\"shutdownPercent\":1")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"shutdownPercent\":2")).c_str());

    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"capacityMah\":0")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"capacityMah\":500")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"capacityMah\":50000")).c_str());
    TEST_ASSERT_EQUAL_STRING(CAPACITY_MSG, batteryReject(battery("\"capacityMah\":499")).c_str());
    TEST_ASSERT_EQUAL_STRING(CAPACITY_MSG, batteryReject(battery("\"capacityMah\":50001")).c_str());

    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"saveReadIntervalSeconds\":10")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"saveReadIntervalSeconds\":300")).c_str());
    TEST_ASSERT_EQUAL_STRING(INTERVAL_MSG, batteryReject(battery("\"saveReadIntervalSeconds\":9")).c_str());
    TEST_ASSERT_EQUAL_STRING(INTERVAL_MSG, batteryReject(battery("\"saveReadIntervalSeconds\":301")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"shutdownGraceSeconds\":10")).c_str());
    TEST_ASSERT_EQUAL_STRING(GRACE_MSG, batteryReject(battery("\"shutdownGraceSeconds\":9")).c_str());
    TEST_ASSERT_EQUAL_STRING(GRACE_MSG, batteryReject(battery("\"shutdownGraceSeconds\":301")).c_str());

    TEST_ASSERT_EQUAL_STRING("", batteryReject("{\"alpaca\":{\"batteryMinPercent\":2}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject("{\"alpaca\":{\"batteryMinPercent\":50}}").c_str());
    TEST_ASSERT_EQUAL_STRING(LIMIT_MSG, batteryReject("{\"alpaca\":{\"batteryMinPercent\":1}}").c_str());
    TEST_ASSERT_EQUAL_STRING(LIMIT_MSG, batteryReject("{\"alpaca\":{\"batteryMinPercent\":51}}").c_str());

    // Wrong types are rejected, never defaulted.
    TEST_ASSERT_TRUE(batteryReject(battery("\"enabled\":\"yes\"")).size() > 0);
    TEST_ASSERT_TRUE(batteryReject(battery("\"capacityMah\":-1")).size() > 0);
    TEST_ASSERT_TRUE(batteryReject(battery("\"lowPercent\":20.5")).size() > 0);
    TEST_ASSERT_TRUE(batteryReject(battery("\"pgPin\":300")).size() > 0);
}

void test_battery_pin_validation()
{
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery(CHARGER)).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"alertPin\":33")).c_str());
    TEST_ASSERT_EQUAL_STRING(PINS_MSG, batteryReject(battery("\"alertPin\":6")).c_str());
    TEST_ASSERT_EQUAL_STRING(PINS_MSG, batteryReject(battery("\"alertPin\":25,\"stat1Pin\":25,\"stat2Pin\":26,\"pgPin\":27")).c_str());
    TEST_ASSERT_EQUAL_STRING(PINS_MSG, batteryReject(battery("\"stat1Pin\":25,\"stat2Pin\":25,\"pgPin\":27")).c_str());
    TEST_ASSERT_EQUAL_STRING(CHARGER_MSG, batteryReject(battery("\"stat1Pin\":25")).c_str());
    TEST_ASSERT_EQUAL_STRING(CHARGER_MSG, batteryReject(battery("\"stat1Pin\":25,\"pgPin\":27")).c_str());
    // PG wakes the device: an RTC GPIO.
    TEST_ASSERT_EQUAL_STRING(PG_MSG, batteryReject(battery("\"stat1Pin\":25,\"stat2Pin\":26,\"pgPin\":23")).c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"stat1Pin\":25,\"stat2Pin\":26,\"pgPin\":4")).c_str());

    // I2C-1 always; I2C-2 once wired.
    TEST_ASSERT_EQUAL_STRING(USED_MSG, batteryReject(battery("\"alertPin\":21")).c_str());
    TEST_ASSERT_EQUAL_STRING(
        USED_MSG, batteryReject("{\"sensor\":{\"i2c2SDA\":32,\"i2c2SCL\":33},\"battery\":{\"alertPin\":33}}").c_str());
    // GPS and rain while enabled.
    TEST_ASSERT_EQUAL_STRING("", batteryReject(battery("\"alertPin\":17")).c_str());
    TEST_ASSERT_EQUAL_STRING(USED_MSG, batteryReject("{\"gps\":{\"enabled\":true},\"battery\":{\"alertPin\":17}}").c_str());
    TEST_ASSERT_EQUAL_STRING(USED_MSG, batteryReject("{\"rain\":{\"enabled\":true},\"battery\":{\"alertPin\":19}}").c_str());
    // Wind: the anemometer's own check names its clash first.
    TEST_ASSERT_EQUAL_STRING(
        "Wind: pin is already used by I2C, GPS or the rain sensor",
        batteryReject(std::string("{\"wind\":{\"enabled\":true},\"battery\":{") + CHARGER + "}}").c_str());
    // Display button (spec 031): its check includes the battery pins.
    TEST_ASSERT_EQUAL_STRING(
        "Display: wake button pin is already in use",
        batteryReject("{\"display\":{\"enabled\":true,\"buttonPin\":33},\"battery\":{\"alertPin\":33}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", batteryReject("{\"display\":{\"enabled\":false,\"buttonPin\":33},\"battery\":{\"alertPin\":33}}").c_str());
}

void test_battery_alerts_part()
{
    Config c = Config::createDefault();
    const std::string main = c.alertsToJson(false, Config::AlertsPart::Main);
    const std::string client = c.alertsToJson(false, Config::AlertsPart::Client);
    // The stored parts as before spec 032 (sizes measured at 3bc6f6a).
    TEST_ASSERT_EQUAL(995, main.size());
    TEST_ASSERT_EQUAL(285, client.size());
    TEST_ASSERT_TRUE(main.find("battery") == std::string::npos);
    TEST_ASSERT_TRUE(client.find("battery") == std::string::npos);

    DynamicJsonDocument doc(4096);
    TEST_ASSERT_FALSE(deserializeJson(doc, c.alertsToJson(false, Config::AlertsPart::Battery)));
    TEST_ASSERT_EQUAL(1, doc.as<JsonObject>().size());
    JsonObject events = doc["events"];
    TEST_ASSERT_EQUAL(6, events.size());
    for (const char *key : EVENT_KEYS)
        TEST_ASSERT_FALSE_MESSAGE(events[key].isNull(), key);

    // As ConfigStore loads them, all three parts spliced into the main document.
    c.alerts.batteryLow = {3, "bike", "Low {battery}", "Left {battery_time_left}"};
    const std::string stored = Config::spliceStored(
        c.toJson(false, false),
        c.alertsToJson(false, Config::AlertsPart::Main),
        c.alertsToJson(false, Config::AlertsPart::Client),
        c.alertsToJson(false, Config::AlertsPart::Battery));
    std::string error;
    const auto loaded = Config::fromJson(stored, nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    TEST_ASSERT_EQUAL_UINT8(3, loaded->alerts.batteryLow.level);
    TEST_ASSERT_EQUAL_STRING("Left {battery_time_left}", loaded->alerts.batteryLow.message.c_str());
    TEST_ASSERT_EQUAL_STRING("bike", loaded->alerts.batteryLow.sound.c_str());
}

void test_battery_worst_cases_fit()
{
    Config c = Config::createDefault();
    c.wifi.ssid = std::string(32, 's');
    c.wifi.password = std::string(63, 'p');
    c.wifi.hostname = std::string(32, 'h');
    c.mqtt.broker = std::string(64, 'b');
    c.mqtt.username = std::string(64, 'u');
    c.mqtt.password = std::string(64, 'p');
    c.mqtt.topic = std::string(64, 't');
    c.mqtt.discoveryPrefix = std::string(32, 'd');
    c.ota.password = std::string(64, 'o');
    c.auth.username = std::string(32, 'a');
    c.auth.password = std::string(64, 'p');
    c.ntp.server1 = std::string(64, 'n');
    c.ntp.server2 = std::string(64, 'm');
    c.ntp.timezone = std::string(64, 'z');
    c.deviceName = std::string(32, 'D');
    c.ble.passkey = "123456";
    c.sensor.i2c2SDA = 25;
    c.sensor.i2c2SCL = 26;
    c.sky.sqmlEnabled = true;
    c.sky.authoritative = SkyRole::Sqml;
    for (SkyRole role : {SkyRole::Sqm, SkyRole::Sqml})
    {
        SkyCalibrationConfig &cal = c.calibration(role);
        cal.enabled = true;
        cal.sqmOffset = -4.987654F;
        cal.darkVisibleOffset = 12345.678F;
        cal.darkFullOffset = 23456.789F;
        cal.darkIrOffset = 3456.789F;
        cal.darkSampleCount = 4294967295U;
        cal.darkCalibratedAt = 9999999999999LL;
        c.averaging(role).windowSeconds = 300;
    }
    c.display.enabled = true;
    c.display.pageCount = Display::PAGE_COUNT;
    for (size_t i = 0; i < Display::PAGE_COUNT; ++i)
        c.display.pages[i] = static_cast<Display::Page>(Display::PAGE_COUNT - 1 - i);
    c.display.cycleSeconds = 60;
    c.display.timeoutSeconds = 1800;
    c.display.buttonPin = 34;
    c.battery.capacityMah = 50000;
    c.battery.saveReadIntervalSeconds = 300;
    c.battery.shutdownGraceSeconds = 300;
    c.battery.alertPin = 33;
    c.battery.stat1Pin = 13;
    c.battery.stat2Pin = 14;
    c.battery.pgPin = 32;

    const std::string title(AlertsConfig::MAX_TEMPLATE_TITLE, 'T');
    const std::string message(AlertsConfig::MAX_TEMPLATE_MESSAGE, 'M');
    for (AlertsConfig::EventSetting *event :
         {&c.alerts.batteryLow,
          &c.alerts.batteryCritical,
          &c.alerts.batteryShutdown,
          &c.alerts.powerRestored,
          &c.alerts.chargeComplete,
          &c.alerts.chargeFault})
        *event = {4, "persistent", title, message};

    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(c.validate(&error), error.c_str());
    char detail[64];
    const size_t size = c.toJson(false, false).size();
    snprintf(detail, sizeof detail, "persisted JSON %u bytes", static_cast<unsigned>(size));
    printf("battery worst case: %s\n", detail);
    TEST_ASSERT_LESS_THAN_size_t_MESSAGE(Config::MAX_PERSISTED_JSON_BYTES, size, detail);
    const size_t part = c.alertsToJson(false, Config::AlertsPart::Battery).size();
    snprintf(detail, sizeof detail, "battery alerts part %u bytes", static_cast<unsigned>(part));
    printf("battery worst case: %s\n", detail);
    TEST_ASSERT_TRUE_MESSAGE(part <= Config::MAX_ALERTS_JSON_BYTES, detail);
}
