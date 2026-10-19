#include <unity.h>

#include <ArduinoJson.h>

#include "DeviceCore.h"

#include <cstring>

// What the OLED may show (specs/031-oled-display, research D4): built from
// the same readings snapshot every interface uses, so stale or faulted
// values are invalid, never numbers.

using namespace SQM;
using SQM::Display::Page;

namespace
{
    constexpr int64_t EPOCH = 1791417600;

    void okSky(SkySensorSnapshot &sky, uint32_t now, float lux)
    {
        sky.initialized = true;
        sky.reading.status = SensorStatus::Ok;
        sky.reading.timestamp = now - 400;
        sky.reading.lux = lux;
        sky.lastUpdate = now - 400;
        sky.diagnostics.integrationMs = 600;
        sky.diagnostics.averagingWindowSeconds = 90;
    }

    SensorSnapshot healthy(uint32_t now)
    {
        SensorSnapshot s;
        okSky(s.skySensors[0], now, 0.0003F);
        s.bmeInitialized = s.mlxInitialized = s.gpsInitialized = true;
        s.bme.status = s.mlx.status = s.gps.status = SensorStatus::Ok;
        s.bme.timestamp = s.mlx.timestamp = s.gps.timestamp = now - 400;
        s.bmeLastUpdate = s.mlxLastUpdate = s.gpsLastUpdate = now - 400;
        s.bme.temperature = 12.0F;
        s.bme.humidity = 60.0F;
        s.bme.dewpoint = 4.5F;
        s.mlx.objectTemp = -25.0F;
        s.mlx.ambientTemp = 12.0F;
        s.gps.hasFix = true;
        s.gps.satellites = 9;
        s.gps.latitude = 51.5;
        s.gps.longitude = -0.12;
        s.dataTimestamp = now - 400;
        return s;
    }

    struct Built
    {
        Display::View view;
        Readings::Snapshot readings;
    };

    Built build(
        SensorSnapshot s,
        const Config &cfg,
        uint32_t now,
        const SafetyStatus &safety = SafetyStatus{},
        const Core::DisplayNetwork &net = Core::DisplayNetwork{})
    {
        Core::derive(s, cfg);
        Built b;
        b.readings = Core::buildReadings(s, cfg, now, EPOCH);
        b.view = Core::displayView(s, b.readings, safety, cfg, net);
        return b;
    }
} // namespace

void test_display_view_sky_follows_the_authoritative_reading(void)
{
    const uint32_t now = 100000;
    Config cfg = Config::createDefault();
    const Built b = build(healthy(now), cfg, now);
    TEST_ASSERT_TRUE(b.view.sky.available);
    TEST_ASSERT_TRUE(b.view.sky.valid);
    TEST_ASSERT_EQUAL_STRING("SQM", b.view.sky.label);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, static_cast<float>(b.readings.sky.sqm), b.view.sky.sqm);
    TEST_ASSERT_EQUAL(b.readings.sky.bortle, b.view.sky.bortle);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, static_cast<float>(b.readings.sky.nelm), b.view.sky.nelm);
    TEST_ASSERT_FALSE(b.view.sky.other.present);

    // Both sensors, SQM-L authoritative: the label follows, the SQM is the other reading.
    cfg.sensor.i2c2SDA = 25;
    cfg.sensor.i2c2SCL = 26;
    cfg.sky.sqmlEnabled = true;
    cfg.sky.authoritative = SkyRole::Sqml;
    SensorSnapshot s = healthy(now);
    okSky(s.skySensors[1], now, 0.0002F);
    const Built both = build(s, cfg, now);
    TEST_ASSERT_EQUAL_STRING("SQM-L", both.view.sky.label);
    TEST_ASSERT_TRUE(both.view.sky.valid);
    TEST_ASSERT_TRUE(both.view.sky.other.present);
    TEST_ASSERT_TRUE(both.view.sky.other.valid);
    TEST_ASSERT_EQUAL_STRING("SQM", both.view.sky.other.label);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, static_cast<float>(both.readings.skySensors[0].sqm), both.view.sky.other.sqm);

    // The other sensor stale: present, not valid.
    s.skySensors[0].reading.timestamp = s.skySensors[0].lastUpdate = 1;
    const Built staleOther = build(s, cfg, now);
    TEST_ASSERT_TRUE(staleOther.view.sky.other.present);
    TEST_ASSERT_FALSE(staleOther.view.sky.other.valid);
    TEST_ASSERT_TRUE(staleOther.view.sky.valid);
}

void test_display_view_stale_or_missing_sky_is_invalid(void)
{
    const uint32_t now = 100000;
    const Config cfg = Config::createDefault();
    SensorSnapshot s = healthy(now);
    s.skySensors[0].reading.timestamp = s.skySensors[0].lastUpdate = 1; // long ago
    Built b = build(s, cfg, now);
    TEST_ASSERT_TRUE(b.readings.light.status == Readings::Status::Stale);
    TEST_ASSERT_TRUE(b.view.sky.available);
    TEST_ASSERT_FALSE(b.view.sky.valid);

    s = healthy(now);
    s.skySensors[0].initialized = false;
    s.skySensors[0].reading.status = SensorStatus::NotInitialized;
    b = build(s, cfg, now);
    TEST_ASSERT_FALSE(b.view.sky.valid);

    // The authoritative sensor off: no Sky page.
    Config off = cfg;
    off.sky.sqmEnabled = false;
    b = build(healthy(now), off, now);
    TEST_ASSERT_FALSE(b.view.sky.available);
    TEST_ASSERT_FALSE(b.view.available(Page::Sky));
}

void test_display_view_groups_and_availability(void)
{
    const uint32_t now = 100000;
    Config cfg = Config::createDefault();
    cfg.gps.enabled = true;
    SafetyStatus safety;
    safety.evaluatedAtMs = now;
    safety.isSafe = false;
    safety.reasons = {"SQM 18.21 < 19.50", "Cloud cover 95% > 90%"};
    Core::DisplayNetwork net;
    net.connected = true;
    net.ip = "192.168.1.128";
    cfg.deviceName = "Roof";
    const Built b = build(healthy(now), cfg, now, safety, net);

    TEST_ASSERT_TRUE(b.view.environment.available);
    TEST_ASSERT_TRUE(b.view.environment.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 12.0F, b.view.environment.temperatureC);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 60.0F, b.view.environment.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 4.5F, b.view.environment.dewpointC);

    TEST_ASSERT_TRUE(b.view.cloud.available);
    TEST_ASSERT_TRUE(b.view.cloud.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, -25.0F, b.view.cloud.skyTemperatureC);
    TEST_ASSERT_EQUAL_STRING(b.readings.clouds.condition, b.view.cloud.condition);

    TEST_ASSERT_TRUE(b.view.safety.known);
    TEST_ASSERT_FALSE(b.view.safety.safe);
    TEST_ASSERT_EQUAL_STRING("SQM 18.21 < 19.50", b.view.safety.firstReason);

    TEST_ASSERT_TRUE(b.view.gps.available);
    TEST_ASSERT_TRUE(b.view.gps.valid);
    TEST_ASSERT_TRUE(b.view.gps.fix);
    TEST_ASSERT_EQUAL_UINT32(9, b.view.gps.satellites);

    // Battery: filled by spec 032; never available before then.
    TEST_ASSERT_FALSE(b.view.battery.available);
    TEST_ASSERT_FALSE(b.view.available(Page::Battery));

    TEST_ASSERT_EQUAL_STRING("Roof", b.view.network.name);
    TEST_ASSERT_TRUE(b.view.network.mode == Display::View::NetMode::Wifi);
    TEST_ASSERT_EQUAL_STRING("192.168.1.128", b.view.network.ip);
    TEST_ASSERT_TRUE(b.view.available(Page::Safety));
    TEST_ASSERT_TRUE(b.view.available(Page::Network));
}

void test_display_view_missing_sensors_and_network_modes(void)
{
    const uint32_t now = 100000;
    Config cfg = Config::createDefault(); // GPS off
    SensorSnapshot s = healthy(now);
    s.bmeInitialized = false;
    s.bme.status = SensorStatus::NotInitialized;
    s.mlx.status = SensorStatus::ReadError;
    Core::DisplayNetwork net;
    net.apMode = true;
    net.ip = "192.168.4.1";
    net.apName = "SQM-Setup";
    const Built b = build(s, cfg, now, SafetyStatus{}, net);
    TEST_ASSERT_FALSE(b.view.environment.available);
    TEST_ASSERT_TRUE(b.view.cloud.available); // detected at boot, failing now
    TEST_ASSERT_FALSE(b.view.cloud.valid);
    TEST_ASSERT_FALSE(b.view.gps.available);
    TEST_ASSERT_FALSE(b.view.safety.known); // never evaluated
    TEST_ASSERT_TRUE(b.view.network.mode == Display::View::NetMode::Ap);
    TEST_ASSERT_EQUAL_STRING("SQM-Setup", b.view.network.apName);
    TEST_ASSERT_EQUAL_STRING("192.168.4.1", b.view.network.ip);

    const Built none = build(s, cfg, now, SafetyStatus{}, Core::DisplayNetwork{});
    TEST_ASSERT_TRUE(none.view.network.mode == Display::View::NetMode::None);
    TEST_ASSERT_EQUAL_STRING("", none.view.network.ip);
}

void test_display_status_document(void)
{
    Config cfg = Config::createDefault();
    DynamicJsonDocument doc(512);
    // No display configured: always present, all off.
    Core::writeDisplayStatus(doc.to<JsonObject>(), cfg, Core::DisplayStatus{});
    JsonObject d = doc["display"];
    TEST_ASSERT_FALSE(d.isNull());
    TEST_ASSERT_FALSE(d["enabled"].as<bool>());
    TEST_ASSERT_FALSE(d["detected"].as<bool>());
    TEST_ASSERT_FALSE(d["on"].as<bool>());
    TEST_ASSERT_TRUE(d.containsKey("page"));
    TEST_ASSERT_TRUE(d["page"].isNull());
    TEST_ASSERT_FALSE(d["nightMode"].as<bool>());

    // Disabled: never detected, whatever the caller passes.
    Core::DisplayStatus state;
    state.detected = true;
    state.on = true;
    state.page = Page::Gps;
    doc.clear();
    Core::writeDisplayStatus(doc.to<JsonObject>(), cfg, state);
    TEST_ASSERT_FALSE(doc["display"]["detected"].as<bool>());
    TEST_ASSERT_FALSE(doc["display"]["on"].as<bool>());
    TEST_ASSERT_TRUE(doc["display"]["page"].isNull());

    // Enabled, lit, at night.
    cfg.display.enabled = true;
    state.nightMode = true;
    doc.clear();
    Core::writeDisplayStatus(doc.to<JsonObject>(), cfg, state);
    d = doc["display"];
    TEST_ASSERT_TRUE(d["enabled"].as<bool>());
    TEST_ASSERT_TRUE(d["detected"].as<bool>());
    TEST_ASSERT_TRUE(d["on"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("gps", d["page"]);
    TEST_ASSERT_TRUE(d["nightMode"].as<bool>());

    // Enabled, not detected: off, no page.
    state.detected = false;
    doc.clear();
    Core::writeDisplayStatus(doc.to<JsonObject>(), cfg, state);
    TEST_ASSERT_FALSE(doc["display"]["detected"].as<bool>());
    TEST_ASSERT_FALSE(doc["display"]["on"].as<bool>());
    TEST_ASSERT_TRUE(doc["display"]["page"].isNull());

    // The settings-dependency fact.
    Deps::Facts facts;
    Core::displayFacts(facts, cfg, true);
    TEST_ASSERT_TRUE(facts.displayDetected);
    cfg.display.enabled = false;
    Core::displayFacts(facts, cfg, true);
    TEST_ASSERT_FALSE(facts.displayDetected);
}
