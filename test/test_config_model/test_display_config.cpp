#include <unity.h>

#include <ArduinoJson.h>

#include <cstdio>
#include <string>

#include "Config.h"

// OLED display settings (specs/031-oled-display, contracts/config.md).

using namespace SQM;
using SQM::Display::Brightness;
using SQM::Display::Page;

namespace
{
    std::string displayReject(const std::string &json, const Config &base)
    {
        std::string error;
        const auto cfg = Config::fromJson(json, &base, &error);
        return cfg ? std::string() : error;
    }

    std::string displayReject(const std::string &json)
    {
        return displayReject(json, Config::createDefault());
    }

    Config withDisplayOn()
    {
        Config cfg = Config::createDefault();
        cfg.display.enabled = true;
        return cfg;
    }

    const char *const PAGES_MSG = "Display: choose 1 to 7 different pages";
    const char *const CYCLE_MSG = "Display: cycle interval must be 2 to 60 seconds";
    const char *const BRIGHTNESS_MSG = "Display: brightness must be low, medium or high";
    const char *const TIMEOUT_MSG = "Display: timeout must be 10 seconds to 30 minutes, or never";
    const char *const PIN_INVALID_MSG = "Display: wake button pin is not a valid GPIO";
    const char *const PIN_USED_MSG = "Display: wake button pin is already in use";
} // namespace

void test_display_defaults()
{
    const Config c = Config::createDefault();
    const DisplayConfig &d = c.display;
    TEST_ASSERT_FALSE(d.enabled);
    TEST_ASSERT_EQUAL_UINT8(1, d.pageCount);
    TEST_ASSERT_TRUE(d.pages[0] == Page::Sky);
    TEST_ASSERT_EQUAL_UINT8(5, d.cycleSeconds);
    TEST_ASSERT_TRUE(d.brightness == Brightness::Medium);
    TEST_ASSERT_EQUAL_UINT16(60, d.timeoutSeconds);
    TEST_ASSERT_TRUE(d.wakeOnBoot);
    TEST_ASSERT_FALSE(d.stayOnAtNight);
    TEST_ASSERT_FALSE(d.rotate180);
    TEST_ASSERT_EQUAL_UINT8(255, d.buttonPin);
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(c.validate(&error), error.c_str());

    DynamicJsonDocument doc(16384);
    TEST_ASSERT_FALSE(deserializeJson(doc, c.toJson()));
    JsonObject display = doc["display"];
    TEST_ASSERT_FALSE(display.isNull());
    TEST_ASSERT_FALSE(display["enabled"].as<bool>());
    TEST_ASSERT_EQUAL(1, display["pages"].size());
    TEST_ASSERT_EQUAL_STRING("sky", display["pages"][0]);
    TEST_ASSERT_EQUAL(5, display["cycleSeconds"].as<int>());
    TEST_ASSERT_EQUAL_STRING("medium", display["brightness"]);
    TEST_ASSERT_EQUAL(60, display["timeoutSeconds"].as<int>());
    TEST_ASSERT_TRUE(display["wakeOnBoot"].as<bool>());
    TEST_ASSERT_FALSE(display["stayOnAtNight"].as<bool>());
    TEST_ASSERT_FALSE(display["rotate180"].as<bool>());
    TEST_ASSERT_EQUAL(255, display["buttonPin"].as<int>());
}

void test_display_json_round_trip()
{
    Config c = withDisplayOn();
    c.display.pageCount = 3;
    c.display.pages[0] = Page::Network;
    c.display.pages[1] = Page::Sky;
    c.display.pages[2] = Page::Safety;
    c.display.cycleSeconds = 12;
    c.display.brightness = Brightness::High;
    c.display.timeoutSeconds = 0;
    c.display.wakeOnBoot = false;
    c.display.stayOnAtNight = true;
    c.display.rotate180 = true;
    c.display.buttonPin = 33;

    std::string error;
    const auto loaded = Config::fromJson(c.toJson(), nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    const DisplayConfig &d = loaded->display;
    TEST_ASSERT_TRUE(d.enabled);
    TEST_ASSERT_EQUAL_UINT8(3, d.pageCount);
    TEST_ASSERT_TRUE(d.pages[0] == Page::Network);
    TEST_ASSERT_TRUE(d.pages[1] == Page::Sky);
    TEST_ASSERT_TRUE(d.pages[2] == Page::Safety);
    TEST_ASSERT_EQUAL_UINT8(12, d.cycleSeconds);
    TEST_ASSERT_TRUE(d.brightness == Brightness::High);
    TEST_ASSERT_EQUAL_UINT16(0, d.timeoutSeconds);
    TEST_ASSERT_FALSE(d.wakeOnBoot);
    TEST_ASSERT_TRUE(d.stayOnAtNight);
    TEST_ASSERT_TRUE(d.rotate180);
    TEST_ASSERT_EQUAL_UINT8(33, d.buttonPin);
}

void test_display_partial_update_keeps_other_keys()
{
    Config base = withDisplayOn();
    base.display.cycleSeconds = 20;
    base.display.rotate180 = true;
    std::string error;
    const auto cfg = Config::fromJson("{\"display\":{\"pages\":[\"gps\",\"sky\"]}}", &base, &error);
    TEST_ASSERT_TRUE_MESSAGE(cfg.has_value(), error.c_str());
    TEST_ASSERT_TRUE(cfg->display.enabled);
    TEST_ASSERT_EQUAL_UINT8(2, cfg->display.pageCount);
    TEST_ASSERT_TRUE(cfg->display.pages[0] == Page::Gps);
    TEST_ASSERT_TRUE(cfg->display.pages[1] == Page::Sky);
    TEST_ASSERT_EQUAL_UINT8(20, cfg->display.cycleSeconds);
    TEST_ASSERT_TRUE(cfg->display.rotate180);
}

void test_display_page_validation()
{
    TEST_ASSERT_EQUAL_STRING(
        "",
        displayReject("{\"display\":{\"pages\":[\"sky\",\"environment\",\"cloud\",\"safety\",\"gps\","
                      "\"battery\",\"network\"]}}")
            .c_str());
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, displayReject("{\"display\":{\"pages\":[]}}").c_str());
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, displayReject("{\"display\":{\"pages\":[\"sky\",\"sky\"]}}").c_str());
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, displayReject("{\"display\":{\"pages\":[\"sky\",\"wind\"]}}").c_str());
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, displayReject("{\"display\":{\"pages\":[\"Sky\"]}}").c_str());
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, displayReject("{\"display\":{\"pages\":[1]}}").c_str());
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, displayReject("{\"display\":{\"pages\":\"sky\"}}").c_str());
    TEST_ASSERT_EQUAL_STRING(
        PAGES_MSG,
        displayReject("{\"display\":{\"pages\":[\"sky\",\"environment\",\"cloud\",\"safety\","
                      "\"gps\",\"battery\",\"network\",\"sky\"]}}")
            .c_str());

    // A config built in code is checked the same way.
    Config bad = Config::createDefault();
    bad.display.pageCount = 0;
    std::string error;
    TEST_ASSERT_FALSE(bad.validate(&error));
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, error.c_str());
    bad.display.pageCount = 2;
    bad.display.pages[0] = Page::Cloud;
    bad.display.pages[1] = Page::Cloud;
    TEST_ASSERT_FALSE(bad.validate(&error));
    TEST_ASSERT_EQUAL_STRING(PAGES_MSG, error.c_str());
}

void test_display_range_validation()
{
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"cycleSeconds\":2}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"cycleSeconds\":60}}").c_str());
    TEST_ASSERT_EQUAL_STRING(CYCLE_MSG, displayReject("{\"display\":{\"cycleSeconds\":1}}").c_str());
    TEST_ASSERT_EQUAL_STRING(CYCLE_MSG, displayReject("{\"display\":{\"cycleSeconds\":61}}").c_str());

    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"brightness\":\"low\"}}").c_str());
    TEST_ASSERT_EQUAL_STRING(BRIGHTNESS_MSG, displayReject("{\"display\":{\"brightness\":\"max\"}}").c_str());
    TEST_ASSERT_EQUAL_STRING(BRIGHTNESS_MSG, displayReject("{\"display\":{\"brightness\":2}}").c_str());

    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"timeoutSeconds\":0}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"timeoutSeconds\":10}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"timeoutSeconds\":1800}}").c_str());
    TEST_ASSERT_EQUAL_STRING(TIMEOUT_MSG, displayReject("{\"display\":{\"timeoutSeconds\":9}}").c_str());
    TEST_ASSERT_EQUAL_STRING(TIMEOUT_MSG, displayReject("{\"display\":{\"timeoutSeconds\":1801}}").c_str());

    // Wrong types are rejected, never defaulted.
    TEST_ASSERT_TRUE(displayReject("{\"display\":{\"enabled\":\"yes\"}}").size() > 0);
    TEST_ASSERT_TRUE(displayReject("{\"display\":{\"cycleSeconds\":-1}}").size() > 0);
    TEST_ASSERT_TRUE(displayReject("{\"display\":{\"timeoutSeconds\":2.5}}").size() > 0);
}

void test_display_button_pin_validation()
{
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"buttonPin\":255}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"buttonPin\":33}}").c_str());
    TEST_ASSERT_EQUAL_STRING(PIN_INVALID_MSG, displayReject("{\"display\":{\"buttonPin\":6}}").c_str());
    TEST_ASSERT_EQUAL_STRING(PIN_INVALID_MSG, displayReject("{\"display\":{\"buttonPin\":40}}").c_str());

    const Config on = withDisplayOn();
    // I2C-1.
    TEST_ASSERT_EQUAL_STRING(PIN_USED_MSG, displayReject("{\"display\":{\"buttonPin\":21}}", on).c_str());
    TEST_ASSERT_EQUAL_STRING(PIN_USED_MSG, displayReject("{\"display\":{\"buttonPin\":22}}", on).c_str());
    // I2C-2 once wired.
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"buttonPin\":25}}", on).c_str());
    TEST_ASSERT_EQUAL_STRING(
        PIN_USED_MSG, displayReject("{\"sensor\":{\"i2c2SDA\":25,\"i2c2SCL\":26},\"display\":{\"buttonPin\":26}}", on).c_str());
    // GPS and rain only while enabled.
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"gps\":{\"enabled\":false},\"display\":{\"buttonPin\":17}}", on).c_str());
    TEST_ASSERT_EQUAL_STRING(PIN_USED_MSG, displayReject("{\"gps\":{\"enabled\":true},\"display\":{\"buttonPin\":17}}", on).c_str());
    TEST_ASSERT_EQUAL_STRING(PIN_USED_MSG, displayReject("{\"rain\":{\"enabled\":true},\"display\":{\"buttonPin\":19}}", on).c_str());
    // Anemometer and vane.
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"wind\":{\"enabled\":false},\"display\":{\"buttonPin\":27}}", on).c_str());
    TEST_ASSERT_EQUAL_STRING(PIN_USED_MSG, displayReject("{\"wind\":{\"enabled\":true},\"display\":{\"buttonPin\":27}}", on).c_str());
    TEST_ASSERT_EQUAL_STRING(
        PIN_USED_MSG, displayReject("{\"wind\":{\"enabled\":true,\"directionEnabled\":true},\"display\":{\"buttonPin\":35}}", on).c_str());
    // A clash doesn't matter while the display is off.
    TEST_ASSERT_EQUAL_STRING("", displayReject("{\"display\":{\"buttonPin\":21}}").c_str());
}

void test_display_worst_case_config_fits()
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
        c.display.pages[i] = static_cast<Page>(Display::PAGE_COUNT - 1 - i);
    c.display.cycleSeconds = 60;
    c.display.brightness = Brightness::Medium;
    c.display.timeoutSeconds = 1800;
    c.display.buttonPin = 33;

    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(c.validate(&error), error.c_str());
    const size_t size = c.toJson(false, false).size();
    char message[64];
    snprintf(message, sizeof message, "persisted JSON %u bytes", static_cast<unsigned>(size));
    TEST_ASSERT_LESS_THAN_size_t_MESSAGE(Config::MAX_PERSISTED_JSON_BYTES, size, message);
}
