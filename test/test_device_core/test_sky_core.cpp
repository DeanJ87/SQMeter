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
