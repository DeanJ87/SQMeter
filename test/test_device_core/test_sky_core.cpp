#include <unity.h>

#include "DeviceCore.h"

#include <cstdio>
#include <string>
#include <vector>

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

namespace
{
    Alpaca::SafetyResult skyVerdict(SensorSnapshot &s, const Config &cfg, uint32_t now)
    {
        // The IR sky sensor answering and clear, so only the sky sensors matter.
        s.mlx.status = SensorStatus::Ok;
        s.mlx.objectTemp = -25.0F;
        s.mlx.ambientTemp = 12.0F;
        s.mlxInitialized = true;
        s.mlxLastUpdate = now - 400;
        s.dataTimestamp = now - 400;
        Core::derive(s, cfg);
        return Alpaca::evaluateSafety(Core::safetyInputs(s, cfg, now), Core::safetyThresholds(cfg));
    }
} // namespace

// Spec 030 D4: only the authoritative sensor is required; no fallback.
void test_sky_safety_uses_the_authoritative_sensor_only()
{
    const uint32_t now = 100000;
    Config cfg = dual(SkyRole::Sqml);
    cfg.alpaca.sqmMinEnabled = true;
    cfg.alpaca.sqmMinSafe = 30.0F; // any reading is below it

    // SQM-L authoritative and faulted, SQM fine: unsafe, limit not evaluated.
    SensorSnapshot s = twoSensors(now);
    s.skySensors[SQML_I].reading.status = SensorStatus::ReadError;
    Alpaca::SafetyResult r = skyVerdict(s, cfg, now);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_EQUAL_STRING("Sensor fault: SQM-L sensor", r.unsafeReasons.front().c_str());
    TEST_ASSERT_FALSE(r.reasonFlags & Alpaca::UnsafeSkyBright);

    // SQM-L authoritative but switched off: the empty copy is a fault too.
    Config off = cfg;
    off.sky.sqmlEnabled = false;
    s = twoSensors(now);
    r = skyVerdict(s, off, now);
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_TRUE(r.reasonFlags & Alpaca::UnsafeSensorFault);

    // The non-authoritative sensor faulting changes nothing.
    cfg.alpaca.sqmMinEnabled = false;
    s = twoSensors(now);
    TEST_ASSERT_TRUE(skyVerdict(s, cfg, now).isSafe);
    s = twoSensors(now);
    s.skySensors[SQM_I].reading.status = SensorStatus::ReadError;
    s.skySensors[SQM_I].lastUpdate = 0;
    TEST_ASSERT_TRUE(skyVerdict(s, cfg, now).isSafe);

    // Freshness follows the authoritative sensor: a stale SQM doesn't matter,
    // a stale SQM-L does.
    s = twoSensors(now);
    s.skySensors[SQM_I].lastUpdate = now - 600000;
    TEST_ASSERT_TRUE(skyVerdict(s, cfg, now).isSafe);
    s = twoSensors(now);
    s.skySensors[SQML_I].lastUpdate = now - 600000;
    r = skyVerdict(s, cfg, now);
    TEST_ASSERT_TRUE(r.reasonFlags & Alpaca::UnsafeStaleData);

    // The limit reason names the sensor.
    cfg.alpaca.sqmMinEnabled = true;
    s = twoSensors(now);
    r = skyVerdict(s, cfg, now);
    TEST_ASSERT_EQUAL_STRING("SQM-L", r.unsafeReasons.front().substr(0, 5).c_str());

    // One sensor: today's reason.
    Config one = Config::createDefault();
    s = twoSensors(now);
    s.skySensors[SQM_I].reading.status = SensorStatus::ReadError;
    r = skyVerdict(s, one, now);
    TEST_ASSERT_EQUAL_STRING("Sensor fault: light sensor", r.unsafeReasons.front().c_str());
    // Two sensors, SQM authoritative.
    s = twoSensors(now);
    s.skySensors[SQM_I].reading.status = SensorStatus::ReadError;
    r = skyVerdict(s, dual(SkyRole::Sqm), now);
    TEST_ASSERT_EQUAL_STRING("Sensor fault: SQM sensor", r.unsafeReasons.front().c_str());
}

// Alpaca skyquality/skybrightness follow the authoritative sensor:
// NotImplemented when it is off, no data when it is faulted.
void test_sky_alpaca_follows_the_authoritative_sensor()
{
    using namespace SQM::Alpaca;
    const uint32_t now = 100000;
    Config cfg = dual(SkyRole::Sqml);
    SensorSnapshot s = twoSensors(now);
    Core::derive(s, cfg);
    ObservingConditionsSnapshot obs = Core::observingConditions(s, cfg, now);
    PropertyResult q = getObservingConditionsProperty("skyquality", obs);
    TEST_ASSERT_TRUE(q.ok);
    TEST_ASSERT_FLOAT_WITHIN(1e-4, SkyQuality::calculate(0.0002F).sqm, q.value);
    TEST_ASSERT_EQUAL_STRING("TSL2591 SQM-L (narrow), calibrated SQM", getSensorDescription("skyquality", obs).value.c_str());

    cfg.sky.sqmlEnabled = false;
    s = twoSensors(now);
    Core::derive(s, cfg);
    obs = Core::observingConditions(s, cfg, now);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getObservingConditionsProperty("skyquality", obs).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getSensorDescription("skybrightness", obs).errorNumber);

    cfg = dual(SkyRole::Sqml);
    s = twoSensors(now);
    s.skySensors[SQML_I].reading.status = SensorStatus::ReadError;
    Core::derive(s, cfg);
    obs = Core::observingConditions(s, cfg, now);
    q = getObservingConditionsProperty("skyquality", obs);
    TEST_ASSERT_FALSE(q.ok);
    TEST_ASSERT_TRUE(q.errorNumber != ALPACA_ERR_NOT_IMPLEMENTED);

    cfg = Config::createDefault();
    s = twoSensors(now);
    Core::derive(s, cfg);
    obs = Core::observingConditions(s, cfg, now);
    TEST_ASSERT_EQUAL_STRING("TSL2591 light sensor", getSensorDescription("skybrightness", obs).value.c_str());
}

// US3 AC2: dark-calibrating one sky sensor writes only its own section.
void test_sky_dark_calibration_writes_one_role()
{
    TSL2591Diagnostics diag{};
    diag.rollingVisible = 3.25F;
    diag.sampleCount = 150;

    Config cfg = dual(SkyRole::Sqm);
    cfg.skyCalibration.darkVisibleOffset = 1.0F;
    cfg.skyCalibration.darkFullOffset = 0.5F;
    const Config before = cfg;
    Core::applyDarkCalibration(cfg, SkyRole::Sqml, diag, 1790000000);
    TEST_ASSERT_EQUAL_FLOAT(3.25F, cfg.skyCalibrationL.darkVisibleOffset);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, cfg.skyCalibrationL.darkFullOffset);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, cfg.skyCalibrationL.darkIrOffset);
    TEST_ASSERT_EQUAL_UINT32(150, cfg.skyCalibrationL.darkSampleCount);
    TEST_ASSERT_TRUE(cfg.skyCalibrationL.darkCalibratedAt == 1790000000);
    // The SQM's section (and everything else) byte-identical.
    Config expect = cfg;
    expect.skyCalibrationL = before.skyCalibrationL;
    TEST_ASSERT_EQUAL_STRING(before.toJson().c_str(), expect.toJson().c_str());

    cfg = before;
    Core::applyDarkCalibration(cfg, SkyRole::Sqm, diag, 1790000001);
    TEST_ASSERT_EQUAL_FLOAT(3.25F, cfg.skyCalibration.darkVisibleOffset);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, cfg.skyCalibration.darkFullOffset);
    expect = cfg;
    expect.skyCalibration = before.skyCalibration;
    TEST_ASSERT_EQUAL_STRING(before.toJson().c_str(), expect.toJson().c_str());
}

namespace
{
    std::string var(const std::vector<std::pair<std::string, std::string>> &vars, const char *name)
    {
        for (const auto &v : vars)
            if (v.first == name)
                return v.second;
        return "<none>";
    }

    std::vector<std::pair<std::string, std::string>> skyVars(SensorSnapshot &s, const Config &cfg, uint32_t now)
    {
        Core::derive(s, cfg);
        const Alpaca::ObservingConditionsSnapshot obs = Core::observingConditions(s, cfg, now);
        const Core::NightState night = Core::night(s, cfg, 1791417600);
        const SafetyStatus status{};
        Alerts::Alert alert;
        return Core::alertVars(
            Core::AlertSources{cfg, obs, night, status, Core::skyReadings(s, cfg, now)}, alert, Core::LocalClock{"23:00", "2026-10-07"});
    }
} // namespace

// Spec 030 D10: each sky sensor's value, their difference and the source.
void test_sky_alert_variables()
{
    const uint32_t now = 100000;
    SensorSnapshot s = twoSensors(now);
    auto vars = skyVars(s, dual(SkyRole::Sqml), now);
    char expected[16];
    std::snprintf(expected, sizeof expected, "%.2f", SkyQuality::calculate(0.0004F).sqm);
    TEST_ASSERT_EQUAL_STRING(expected, var(vars, "sqm_wide").c_str());
    std::snprintf(expected, sizeof expected, "%.2f", SkyQuality::calculate(0.0002F).sqm);
    TEST_ASSERT_EQUAL_STRING(expected, var(vars, "sqm_l").c_str());
    TEST_ASSERT_EQUAL_STRING(expected, var(vars, "sqm").c_str()); // authoritative = SQM-L
    std::snprintf(expected, sizeof expected, "%+.2f", SkyQuality::calculate(0.0002F).sqm - SkyQuality::calculate(0.0004F).sqm);
    TEST_ASSERT_EQUAL_STRING(expected, var(vars, "sqm_delta").c_str());
    TEST_ASSERT_EQUAL_STRING("+", var(vars, "sqm_delta").substr(0, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("SQM-L", var(vars, "sqm_source").c_str());

    // A sensor without a reading reads like {sqm} without one; so does the difference.
    s = twoSensors(now);
    s.skySensors[SQML_I].reading.status = SensorStatus::ReadError;
    vars = skyVars(s, dual(SkyRole::Sqm), now);
    TEST_ASSERT_EQUAL_STRING("--", var(vars, "sqm_l").c_str());
    TEST_ASSERT_EQUAL_STRING("--", var(vars, "sqm_delta").c_str());
    TEST_ASSERT_TRUE(var(vars, "sqm_wide") != "--");
    TEST_ASSERT_EQUAL_STRING("SQM", var(vars, "sqm_source").c_str());

    // One sensor: the SQM-L's are empty.
    s = twoSensors(now);
    vars = skyVars(s, Config::createDefault(), now);
    TEST_ASSERT_EQUAL_STRING("--", var(vars, "sqm_l").c_str());
    TEST_ASSERT_EQUAL_STRING(var(vars, "sqm").c_str(), var(vars, "sqm_wide").c_str());
}

// Sensor fault/recovered names (D10): "Light sensor" with one sky sensor,
// "SQM sensor" and "SQM-L sensor" with two.
void test_sky_alert_sensor_slots()
{
    const uint32_t now = 100000;
    auto slots = [now](SensorSnapshot &s, const Config &cfg)
    {
        Core::derive(s, cfg);
        const Alpaca::ObservingConditionsSnapshot obs = Core::observingConditions(s, cfg, now);
        const Core::NightState night = Core::night(s, cfg, 1791417600);
        const SafetyStatus status{};
        return Core::alertInputs(Core::AlertSources{cfg, obs, night, status}, s, now);
    };
    TEST_ASSERT_EQUAL_size_t(6, Alerts::SENSOR_COUNT);

    SensorSnapshot s = twoSensors(now);
    Alerts::AlertInputs in = slots(s, Config::createDefault());
    TEST_ASSERT_EQUAL_STRING("Light sensor", in.sensors[0].name);
    TEST_ASSERT_TRUE(in.sensors[0].enabled);
    TEST_ASSERT_TRUE(in.sensors[0].healthy);
    TEST_ASSERT_FALSE(in.sensors[5].enabled);

    s = twoSensors(now);
    s.skySensors[SQML_I].reading.status = SensorStatus::ReadError;
    in = slots(s, dual(SkyRole::Sqm));
    TEST_ASSERT_EQUAL_STRING("SQM sensor", in.sensors[0].name);
    TEST_ASSERT_TRUE(in.sensors[0].healthy);
    TEST_ASSERT_EQUAL_STRING("SQM-L sensor", in.sensors[5].name);
    TEST_ASSERT_TRUE(in.sensors[5].enabled);
    TEST_ASSERT_FALSE(in.sensors[5].healthy);

    // The SQM switched off: its slot is not watched.
    Config noSqm = dual(SkyRole::Sqml);
    noSqm.sky.sqmEnabled = false;
    s = twoSensors(now);
    in = slots(s, noSqm);
    TEST_ASSERT_FALSE(in.sensors[0].enabled);
    TEST_ASSERT_TRUE(in.sensors[5].healthy);
}
