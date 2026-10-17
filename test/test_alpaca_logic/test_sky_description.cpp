#include <unity.h>

#include "ObservingConditionsMapper.h"

#include <string>

// Spec 030 contracts/integrations.md: SensorDescription names the sky sensor.

using namespace SQM;
using namespace SQM::Alpaca;

namespace
{
    std::string describe(const char *name, SkyRole role, bool named)
    {
        ObservingConditionsSnapshot obs;
        obs.skyLight.present = true;
        obs.skyLight.valid = true;
        obs.skyRole = role;
        obs.skyNamed = named;
        const StringResult r = getSensorDescription(name, obs);
        TEST_ASSERT_TRUE(r.ok);
        return r.value;
    }
} // namespace

void test_sky_sensor_descriptions()
{
    TEST_ASSERT_EQUAL_STRING("TSL2591 light sensor", describe("skybrightness", SkyRole::Sqm, false).c_str());
    TEST_ASSERT_EQUAL_STRING("TSL2591 light sensor, calibrated SQM", describe("skyquality", SkyRole::Sqm, false).c_str());
    TEST_ASSERT_EQUAL_STRING("TSL2591 SQM (wide)", describe("skybrightness", SkyRole::Sqm, true).c_str());
    TEST_ASSERT_EQUAL_STRING("TSL2591 SQM (wide), calibrated SQM", describe("SkyQuality", SkyRole::Sqm, true).c_str());
    TEST_ASSERT_EQUAL_STRING("TSL2591 SQM-L (narrow)", describe("skybrightness", SkyRole::Sqml, true).c_str());
    TEST_ASSERT_EQUAL_STRING("TSL2591 SQM-L (narrow), calibrated SQM", describe("skyquality", SkyRole::Sqml, false).c_str());
    // Other sensors are unaffected.
    TEST_ASSERT_EQUAL_STRING(
        "MLX90614 IR thermometer",
        []
        {
            ObservingConditionsSnapshot obs;
            obs.irSky.present = true;
            obs.skyRole = SkyRole::Sqml;
            obs.skyNamed = true;
            return getSensorDescription("skytemperature", obs).value;
        }()
            .c_str());
}
