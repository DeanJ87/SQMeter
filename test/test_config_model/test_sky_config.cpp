#include <unity.h>

#include <ArduinoJson.h>

#include <cmath>
#include <string>

#include "Config.h"

// Two sky sensors (specs/030-dual-sky-sensors, contracts/config.md).

using namespace SQM;

namespace
{
    std::string skyReject(const std::string &json, const Config &base)
    {
        std::string error;
        const auto cfg = Config::fromJson(json, &base, &error);
        return cfg ? std::string() : error;
    }

    std::string skyReject(const std::string &json)
    {
        return skyReject(json, Config::createDefault());
    }

    Config withI2c2(uint8_t sda, uint8_t scl)
    {
        Config cfg = Config::createDefault();
        cfg.sensor.i2c2SDA = sda;
        cfg.sensor.i2c2SCL = scl;
        return cfg;
    }

    void fillLongStrings(Config &c)
    {
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
    }
} // namespace

void test_sky_defaults()
{
    const Config c = Config::createDefault();
    TEST_ASSERT_EQUAL_UINT8(255, c.sensor.i2c2SDA);
    TEST_ASSERT_EQUAL_UINT8(255, c.sensor.i2c2SCL);
    TEST_ASSERT_TRUE(c.sky.sqmEnabled);
    TEST_ASSERT_FALSE(c.sky.sqmlEnabled);
    TEST_ASSERT_TRUE(c.sky.authoritative == SkyRole::Sqm);
    TEST_ASSERT_TRUE(c.sky.showOther);
    TEST_ASSERT_EQUAL_UINT16(90, c.skyAveragingL.windowSeconds);
    TEST_ASSERT_FALSE(c.skyCalibrationL.enabled);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, c.skyCalibrationL.sqmOffset);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, c.skyCalibrationL.darkVisibleOffset);
    TEST_ASSERT_EQUAL_UINT32(0, c.skyCalibrationL.darkSampleCount);
    TEST_ASSERT_TRUE(c.skyCalibrationL.darkCalibratedAt == 0);

    // Helpers pick each role's section.
    TEST_ASSERT_EQUAL_PTR(&c.skyAveraging, &c.averaging(SkyRole::Sqm));
    TEST_ASSERT_EQUAL_PTR(&c.skyAveragingL, &c.averaging(SkyRole::Sqml));
    TEST_ASSERT_EQUAL_PTR(&c.skyCalibration, &c.calibration(SkyRole::Sqm));
    TEST_ASSERT_EQUAL_PTR(&c.skyCalibrationL, &c.calibration(SkyRole::Sqml));
    TEST_ASSERT_TRUE(c.skyEnabled(SkyRole::Sqm));
    TEST_ASSERT_FALSE(c.skyEnabled(SkyRole::Sqml));
}

void test_sky_json_round_trip()
{
    Config c = withI2c2(25, 26);
    c.sky.sqmlEnabled = true;
    c.sky.authoritative = SkyRole::Sqml;
    c.sky.showOther = false;
    c.skyAveragingL.windowSeconds = 120;
    c.skyCalibrationL.enabled = true;
    c.skyCalibrationL.sqmOffset = -0.4F;
    c.skyCalibrationL.darkVisibleOffset = 1.5F;
    c.skyCalibrationL.darkSampleCount = 40;
    c.skyCalibrationL.darkCalibratedAt = 1760000000;

    const std::string json = c.toJson();
    DynamicJsonDocument doc(16384);
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    TEST_ASSERT_EQUAL(25, doc["sensor"]["i2c2SDA"].as<int>());
    TEST_ASSERT_EQUAL(26, doc["sensor"]["i2c2SCL"].as<int>());
    TEST_ASSERT_EQUAL_STRING("sqml", doc["sky"]["authoritative"].as<const char *>());
    TEST_ASSERT_TRUE(doc["sky"]["sqmEnabled"].as<bool>());
    TEST_ASSERT_TRUE(doc["sky"]["sqmlEnabled"].as<bool>());
    TEST_ASSERT_FALSE(doc["sky"]["showOther"].as<bool>());
    TEST_ASSERT_EQUAL(120, doc["skyAveragingL"]["windowSeconds"].as<int>());
    TEST_ASSERT_TRUE(doc["skyCalibrationL"]["enabled"].as<bool>());

    std::string error;
    const auto loaded = Config::fromJson(json, nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    TEST_ASSERT_EQUAL_UINT8(25, loaded->sensor.i2c2SDA);
    TEST_ASSERT_EQUAL_UINT8(26, loaded->sensor.i2c2SCL);
    TEST_ASSERT_TRUE(loaded->sky.sqmlEnabled);
    TEST_ASSERT_TRUE(loaded->sky.authoritative == SkyRole::Sqml);
    TEST_ASSERT_FALSE(loaded->sky.showOther);
    TEST_ASSERT_EQUAL_UINT16(120, loaded->skyAveragingL.windowSeconds);
    TEST_ASSERT_TRUE(loaded->skyCalibrationL.enabled);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, -0.4F, loaded->skyCalibrationL.sqmOffset);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.5F, loaded->skyCalibrationL.darkVisibleOffset);
    TEST_ASSERT_EQUAL_UINT32(40, loaded->skyCalibrationL.darkSampleCount);
    TEST_ASSERT_TRUE(loaded->skyCalibrationL.darkCalibratedAt == 1760000000);
    // The SQM's sections are untouched by the SQM-L's.
    TEST_ASSERT_EQUAL_UINT16(90, loaded->skyAveraging.windowSeconds);
    TEST_ASSERT_FALSE(loaded->skyCalibration.enabled);
}

void test_sky_validation_messages()
{
    // I2C-2 pair rule.
    TEST_ASSERT_EQUAL_STRING("Second I2C bus pins are invalid", skyReject("{\"sensor\":{\"i2c2SDA\":25}}").c_str());
    TEST_ASSERT_EQUAL_STRING("Second I2C bus pins are invalid", skyReject("{\"sensor\":{\"i2c2SDA\":25,\"i2c2SCL\":25}}").c_str());
    TEST_ASSERT_EQUAL_STRING("Second I2C bus pins are invalid", skyReject("{\"sensor\":{\"i2c2SDA\":6,\"i2c2SCL\":25}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"sensor\":{\"i2c2SDA\":25,\"i2c2SCL\":26}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"sensor\":{\"i2c2SDA\":255,\"i2c2SCL\":255}}").c_str());

    // Not used by I2C-1, GPS, rain (when on).
    const char *inUse = "Second I2C bus pins are already in use";
    TEST_ASSERT_EQUAL_STRING(inUse, skyReject("{\"sensor\":{\"i2c2SDA\":21,\"i2c2SCL\":26}}").c_str());
    TEST_ASSERT_EQUAL_STRING(inUse, skyReject("{\"sensor\":{\"i2c2SDA\":25,\"i2c2SCL\":22}}").c_str());
    TEST_ASSERT_EQUAL_STRING(inUse, skyReject("{\"gps\":{\"enabled\":true},\"sensor\":{\"i2c2SDA\":17,\"i2c2SCL\":26}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"sensor\":{\"i2c2SDA\":17,\"i2c2SCL\":16}}").c_str()); // GPS off
    TEST_ASSERT_EQUAL_STRING(inUse, skyReject("{\"rain\":{\"enabled\":true},\"sensor\":{\"i2c2SDA\":18,\"i2c2SCL\":26}}").c_str());
    // Anemometer (when on): the wind pin message covers I2C-2.
    TEST_ASSERT_EQUAL_STRING(
        "Wind: pin is already used by I2C, GPS or the rain sensor",
        skyReject("{\"wind\":{\"enabled\":true},\"sensor\":{\"i2c2SDA\":27,\"i2c2SCL\":26}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"sensor\":{\"i2c2SDA\":27,\"i2c2SCL\":26}}").c_str()); // wind off

    // SQM-L needs the pins.
    TEST_ASSERT_EQUAL_STRING("SQM-L needs the second I2C bus pins", skyReject("{\"sky\":{\"sqmlEnabled\":true}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"sky\":{\"sqmlEnabled\":true}}", withI2c2(25, 26)).c_str());

    // No sky sensor at all is allowed.
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"sky\":{\"sqmEnabled\":false}}").c_str());

    // Authoritative.
    const char *auth = "Authoritative sky reading must be sqm or sqml";
    TEST_ASSERT_EQUAL_STRING(auth, skyReject("{\"sky\":{\"authoritative\":\"SQM-L\"}}").c_str());
    TEST_ASSERT_EQUAL_STRING(auth, skyReject("{\"sky\":{\"authoritative\":1}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"sky\":{\"authoritative\":\"sqml\"}}").c_str());

    // SQM-L averaging and calibration ranges.
    TEST_ASSERT_EQUAL_STRING("SQM-L averaging window is invalid", skyReject("{\"skyAveragingL\":{\"windowSeconds\":9}}").c_str());
    TEST_ASSERT_EQUAL_STRING("SQM-L averaging window is invalid", skyReject("{\"skyAveragingL\":{\"windowSeconds\":301}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"skyAveragingL\":{\"windowSeconds\":10}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"skyAveragingL\":{\"windowSeconds\":300}}").c_str());
    TEST_ASSERT_EQUAL_STRING("SQM-L calibration offset is invalid", skyReject("{\"skyCalibrationL\":{\"sqmOffset\":5.1}}").c_str());
    TEST_ASSERT_EQUAL_STRING("SQM-L calibration offset is invalid", skyReject("{\"skyCalibrationL\":{\"sqmOffset\":-5.1}}").c_str());
    TEST_ASSERT_EQUAL_STRING("", skyReject("{\"skyCalibrationL\":{\"sqmOffset\":-5}}").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "SQM-L dark calibration offsets are invalid", skyReject("{\"skyCalibrationL\":{\"darkVisibleOffset\":-0.1}}").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "SQM-L dark calibration offsets are invalid", skyReject("{\"skyCalibrationL\":{\"darkIrOffset\":-1}}").c_str());

    Config bad = Config::createDefault();
    bad.skyCalibrationL.darkFullOffset = INFINITY;
    std::string error;
    TEST_ASSERT_FALSE(bad.validate(&error));
    TEST_ASSERT_EQUAL_STRING("SQM-L dark calibration offsets are invalid", error.c_str());

    // Out-of-range or fractional unsigned values are rejected up front.
    TEST_ASSERT_TRUE(skyReject("{\"skyAveragingL\":{\"windowSeconds\":-1}}").size() > 0);
    TEST_ASSERT_TRUE(skyReject("{\"sensor\":{\"i2c2SDA\":300,\"i2c2SCL\":26}}").size() > 0);
}

void test_sky_worst_case_config_fits()
{
    Config c = withI2c2(25, 26);
    fillLongStrings(c);
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
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(c.validate(&error), error.c_str());
    const size_t size = c.toJson(false, false).size();
    char message[64];
    snprintf(message, sizeof message, "persisted JSON %u bytes", static_cast<unsigned>(size));
    TEST_ASSERT_LESS_THAN_size_t_MESSAGE(Config::MAX_PERSISTED_JSON_BYTES, size, message);
}
