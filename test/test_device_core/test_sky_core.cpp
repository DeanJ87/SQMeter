#include <unity.h>

#include "DeviceCore.h"

// Two sky sensors (spec 030): the authoritative copy (research D3).

using namespace SQM;

namespace
{
    constexpr size_t SQM_I = 0;
    constexpr size_t SQML_I = 1;

    void okSky(SkySensorSnapshot &sky, uint32_t now, float lux, const char *gain)
    {
        sky.initialized = true;
        sky.reading.status = SensorStatus::Ok;
        sky.reading.timestamp = now - 400;
        sky.reading.lux = lux;
        sky.reading.rawSqm = 20.0F;
        sky.lastUpdate = now - 400;
        sky.diagnostics.gainName = gain;
        sky.diagnostics.integrationMs = 600;
        sky.diagnostics.averagingWindowSeconds = 90;
    }

    SensorSnapshot twoSensors(uint32_t now)
    {
        SensorSnapshot s;
        okSky(s.skySensors[SQM_I], now, 0.0004F, "max");
        okSky(s.skySensors[SQML_I], now, 0.0002F, "high");
        return s;
    }

    Config dual(SkyRole authoritative)
    {
        Config c = Config::createDefault();
        c.sensor.i2c2SDA = 25;
        c.sensor.i2c2SCL = 26;
        c.sky.sqmlEnabled = true;
        c.sky.authoritative = authoritative;
        return c;
    }

    void assertCopyOf(const SensorSnapshot &s, size_t index)
    {
        const SkySensorSnapshot &src = s.skySensors[index];
        TEST_ASSERT_EQUAL_FLOAT(src.reading.lux, s.tsl.lux);
        TEST_ASSERT_TRUE(src.reading.status == s.tsl.status);
        TEST_ASSERT_EQUAL_UINT32(src.reading.timestamp, s.tsl.timestamp);
        TEST_ASSERT_EQUAL_STRING(src.diagnostics.gainName, s.tslDiagnostics.gainName);
        TEST_ASSERT_EQUAL(src.initialized, s.tslInitialized);
        TEST_ASSERT_EQUAL_UINT32(src.lastUpdate, s.tslLastUpdate);
        TEST_ASSERT_EQUAL_FLOAT(src.metrics.sqm, s.sky.sqm);
        TEST_ASSERT_EQUAL_FLOAT(SkyQuality::calculate(src.reading.lux).sqm, s.sky.sqm);
    }

    void assertEmptyCopy(const SensorSnapshot &s)
    {
        TEST_ASSERT_TRUE(s.tsl.status == SensorStatus::NotInitialized);
        TEST_ASSERT_EQUAL_UINT32(0, s.tsl.timestamp);
        TEST_ASSERT_EQUAL_FLOAT(0.0F, s.tsl.lux);
        TEST_ASSERT_FALSE(s.tslInitialized);
        TEST_ASSERT_EQUAL_UINT32(0, s.tslLastUpdate);
        TEST_ASSERT_NULL(s.tslDiagnostics.gainName);
        TEST_ASSERT_EQUAL_FLOAT(0.0F, s.sky.sqm);
        TEST_ASSERT_EQUAL_FLOAT(0.0F, s.sky.lux);
    }
} // namespace

void test_sky_copy_follows_the_authoritative_role()
{
    const uint32_t now = 100000;
    for (SkyRole role : {SkyRole::Sqm, SkyRole::Sqml})
    {
        SensorSnapshot s = twoSensors(now);
        Core::derive(s, dual(role));
        TEST_ASSERT_TRUE(s.skySource == role);
        assertCopyOf(s, skyRoleIndex(role));
        // Both roles carry their own metrics.
        TEST_ASSERT_EQUAL_FLOAT(SkyQuality::calculate(0.0004F).sqm, s.skySensors[SQM_I].metrics.sqm);
        TEST_ASSERT_EQUAL_FLOAT(SkyQuality::calculate(0.0002F).sqm, s.skySensors[SQML_I].metrics.sqm);
        TEST_ASSERT_TRUE(s.skySensors[SQM_I].enabled);
        TEST_ASSERT_TRUE(s.skySensors[SQML_I].enabled);
    }
}

// FR-010: no fallback. A disabled authoritative sensor gives the empty
// not-ok copy; a faulted one gives its own fault, never the other's values.
void test_sky_copy_never_falls_back()
{
    const uint32_t now = 100000;

    // SQM-L authoritative but switched off, SQM fine.
    Config cfg = dual(SkyRole::Sqml);
    cfg.sky.sqmlEnabled = false;
    SensorSnapshot s = twoSensors(now);
    Core::derive(s, cfg);
    TEST_ASSERT_TRUE(s.skySource == SkyRole::Sqml);
    TEST_ASSERT_FALSE(s.skySensors[SQML_I].enabled);
    assertEmptyCopy(s);

    // SQM authoritative but switched off, SQM-L fine.
    cfg = dual(SkyRole::Sqm);
    cfg.sky.sqmEnabled = false;
    s = twoSensors(now);
    Core::derive(s, cfg);
    TEST_ASSERT_TRUE(s.skySource == SkyRole::Sqm);
    assertEmptyCopy(s);

    // Authoritative faulted, the other ok: the fault is copied.
    for (SkyRole role : {SkyRole::Sqm, SkyRole::Sqml})
    {
        s = twoSensors(now);
        SkySensorSnapshot &bad = s.skySensors[skyRoleIndex(role)];
        bad.reading.status = SensorStatus::ReadError;
        bad.reading.lux = 0.0F;
        bad.initialized = false;
        Core::derive(s, dual(role));
        TEST_ASSERT_TRUE(s.tsl.status == SensorStatus::ReadError);
        TEST_ASSERT_FALSE(s.tslInitialized);
        TEST_ASSERT_EQUAL_FLOAT(0.0F, s.tsl.lux);
        const size_t other = role == SkyRole::Sqm ? SQML_I : SQM_I;
        TEST_ASSERT_TRUE(s.tsl.lux != s.skySensors[other].reading.lux);
    }

    // The non-authoritative sensor faulting changes nothing in the copy.
    s = twoSensors(now);
    s.skySensors[SQML_I].reading.status = SensorStatus::ReadError;
    Core::derive(s, dual(SkyRole::Sqm));
    assertCopyOf(s, SQM_I);
}

// SC-001: with one sensor (every existing device) the copy is today's.
void test_sky_copy_one_sensor_is_unchanged()
{
    const uint32_t now = 100000;
    SensorSnapshot s;
    okSky(s.skySensors[SQM_I], now, 0.0003F, "max");
    const Config cfg = Config::createDefault();
    Core::derive(s, cfg);
    assertCopyOf(s, SQM_I);
    TEST_ASSERT_TRUE(s.skySource == SkyRole::Sqm);
    TEST_ASSERT_FALSE(s.skySensors[SQML_I].enabled);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, s.skySensors[SQML_I].metrics.sqm);

    // A TSL2591 that never answered stays "missing".
    SensorSnapshot missing;
    Core::derive(missing, cfg);
    TEST_ASSERT_TRUE(missing.tsl.status == SensorStatus::NotInitialized);
    TEST_ASSERT_FALSE(missing.tslInitialized);
}

namespace
{
    Readings::Snapshot readingsFor(SensorSnapshot &s, const Config &cfg, uint32_t now)
    {
        Core::derive(s, cfg);
        return Core::buildReadings(s, cfg, now, 1791417600);
    }
} // namespace

// Each enabled sensor's own status; the light/sky groups follow the authoritative one.
void test_sky_readings_per_role_status()
{
    const uint32_t now = 100000;
    const Config cfg = dual(SkyRole::Sqm);
    const uint32_t staleAfter = cfg.sensor.readIntervalMs + Core::SENSOR_STALE_GRACE_MS;

    SensorSnapshot s = twoSensors(now);
    Readings::Snapshot r = readingsFor(s, cfg, now);
    TEST_ASSERT_TRUE(r.source == SkyRole::Sqm);
    for (size_t i = 0; i < SKY_ROLE_COUNT; ++i)
    {
        TEST_ASSERT_TRUE(r.skySensors[i].present);
        TEST_ASSERT_TRUE(r.skySensors[i].status == Readings::Status::Ok);
        TEST_ASSERT_EQUAL_UINT32(400, r.skySensors[i].ageMs);
        TEST_ASSERT_FLOAT_WITHIN(1e-6F, s.skySensors[i].metrics.sqm, static_cast<float>(r.skySensors[i].sqm));
    }
    TEST_ASSERT_EQUAL_STRING("max", r.skySensors[SQM_I].gain);
    TEST_ASSERT_EQUAL_STRING("high", r.skySensors[SQML_I].gain);
    TEST_ASSERT_EQUAL_UINT32(600, r.skySensors[SQML_I].integrationMs);

    // SQM-L missing (never answered), error, stale; SQM unaffected.
    s = twoSensors(now);
    s.skySensors[SQML_I].initialized = false;
    s.skySensors[SQML_I].reading.status = SensorStatus::NotInitialized;
    r = readingsFor(s, cfg, now);
    TEST_ASSERT_TRUE(r.skySensors[SQML_I].status == Readings::Status::Missing);
    TEST_ASSERT_TRUE(r.skySensors[SQM_I].status == Readings::Status::Ok);
    TEST_ASSERT_TRUE(r.light.status == Readings::Status::Ok);

    s = twoSensors(now);
    s.skySensors[SQML_I].reading.status = SensorStatus::ReadError;
    r = readingsFor(s, cfg, now);
    TEST_ASSERT_TRUE(r.skySensors[SQML_I].status == Readings::Status::Error);

    s = twoSensors(now);
    s.skySensors[SQML_I].lastUpdate = now - staleAfter - 1;
    r = readingsFor(s, cfg, now);
    TEST_ASSERT_TRUE(r.skySensors[SQML_I].status == Readings::Status::Stale);
    s.skySensors[SQML_I].lastUpdate = now - staleAfter;
    r = readingsFor(s, cfg, now);
    TEST_ASSERT_TRUE(r.skySensors[SQML_I].status == Readings::Status::Ok);

    // SQM-L authoritative and stale: light/sky stale too, SQM still ok.
    const Config cfgL = dual(SkyRole::Sqml);
    s = twoSensors(now);
    s.skySensors[SQML_I].lastUpdate = now - staleAfter - 1;
    r = readingsFor(s, cfgL, now);
    TEST_ASSERT_TRUE(r.source == SkyRole::Sqml);
    TEST_ASSERT_TRUE(r.light.status == Readings::Status::Stale);
    TEST_ASSERT_TRUE(r.skySensors[SQM_I].status == Readings::Status::Ok);

    // Disabled sensor: no entry.
    s = twoSensors(now);
    r = readingsFor(s, Config::createDefault(), now);
    TEST_ASSERT_TRUE(r.skySensors[SQM_I].present);
    TEST_ASSERT_FALSE(r.skySensors[SQML_I].present);
}

// /api/status: skySensors per enabled sensor (contracts/status-and-routes.md);
// sensors.light and diagnostics.light follow the authoritative sensor.
void test_sky_status_documents()
{
    const uint32_t now = 100000;
    Config cfg = dual(SkyRole::Sqml);
    SensorSnapshot s = twoSensors(now);
    s.skySensors[SQM_I].initialized = false;
    s.skySensors[SQM_I].reading.status = SensorStatus::NotInitialized;
    s.skySensors[SQML_I].diagnostics.sampleCount = 150;
    s.skySensors[SQML_I].diagnostics.darkVisibleOffset = 1.5F;
    s.skySensors[SQML_I].diagnostics.nightMode = true;
    const Readings::Snapshot r = readingsFor(s, cfg, now);

    DynamicJsonDocument doc(4096);
    JsonObject root = doc.to<JsonObject>();
    Core::writeSkySensorStatus(root, s, r);
    Core::writeSensorHealth(root.createNestedObject("sensors"), r, cfg);
    Core::writeDiagnostics(root.createNestedObject("diagnostics"), s, cfg, now);

    JsonObject sqm = root["skySensors"]["sqm"];
    TEST_ASSERT_EQUAL_STRING("missing", sqm["status"]);
    TEST_ASSERT_FALSE(sqm["detected"].as<bool>());
    TEST_ASSERT_FALSE(sqm.containsKey("ageMs"));
    TEST_ASSERT_FALSE(sqm.containsKey("gain"));
    JsonObject sqml = root["skySensors"]["sqml"];
    TEST_ASSERT_EQUAL_STRING("ok", sqml["status"]);
    TEST_ASSERT_EQUAL(400, sqml["ageMs"].as<int>());
    TEST_ASSERT_TRUE(sqml["detected"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("high", sqml["gain"]);
    TEST_ASSERT_EQUAL(600, sqml["integrationMs"].as<int>());
    TEST_ASSERT_FALSE(sqml["saturated"].as<bool>());
    TEST_ASSERT_TRUE(sqml["nightMode"].as<bool>());
    TEST_ASSERT_EQUAL(150, sqml["sampleCount"].as<int>());
    TEST_ASSERT_EQUAL(Core::windowSamples(s.skySensors[SQML_I].diagnostics), sqml["windowSamples"].as<int>());
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 1.5F, sqml["darkVisibleOffset"].as<float>());

    // The authoritative sensor (SQM-L) in sensors.light / diagnostics.light.
    TEST_ASSERT_EQUAL_STRING("ok", root["sensors"]["light"]["status"]);
    TEST_ASSERT_EQUAL(150, root["diagnostics"]["light"]["sampleCount"].as<int>());

    // One sensor enabled: only its entry; none enabled: no skySensors.
    cfg = Config::createDefault();
    s = twoSensors(now);
    Readings::Snapshot one = readingsFor(s, cfg, now);
    doc.clear();
    root = doc.to<JsonObject>();
    Core::writeSkySensorStatus(root, s, one);
    TEST_ASSERT_TRUE(root["skySensors"].containsKey("sqm"));
    TEST_ASSERT_FALSE(root["skySensors"].containsKey("sqml"));

    cfg.sky.sqmEnabled = false;
    one = readingsFor(s, cfg, now);
    doc.clear();
    root = doc.to<JsonObject>();
    Core::writeSkySensorStatus(root, s, one);
    TEST_ASSERT_FALSE(root.containsKey("skySensors"));
}
