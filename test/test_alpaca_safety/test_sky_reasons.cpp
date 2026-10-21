#include <unity.h>

#include "SafetyEvaluator.h"

#include <string>

// Spec 030 research D4: literal reasons per sky role, so the device message
// catalogue gets fixed templates.

using namespace SQM;
using namespace SQM::Alpaca;

namespace
{
    SafetyInputs skyOk()
    {
        SafetyInputs in;
        in.hasEverHadGoodData = true;
        in.secondsSinceLastGoodData = 1;
        in.sqm = 18.21F;
        in.cloudCoverPercent = 5.0F;
        in.humidityPercent = 50.0F;
        in.temperatureC = 10.0F;
        in.dewpointC = 2.0F;
        return in;
    }

    SafetyThresholds sqmLimit()
    {
        SafetyThresholds t;
        t.sqmMinEnabled = true;
        t.sqmMinSafe = 19.5F;
        return t;
    }

    std::string firstReason(const SafetyInputs &in, const SafetyThresholds &t)
    {
        const SafetyResult r = evaluateSafety(in, t);
        return r.unsafeReasons.empty() ? std::string() : r.unsafeReasons.front();
    }

    SafetyInputs faulted(SkyRole role, bool named, bool ir)
    {
        SafetyInputs in = skyOk();
        in.skyRole = role;
        in.skyNamed = named;
        in.requiredSensorFault = true;
        in.skyLightFault = true;
        in.irSkyFault = ir;
        return in;
    }
} // namespace

void test_sky_fault_reasons_per_role()
{
    const SafetyThresholds t;
    // One sensor (every existing device): unchanged.
    TEST_ASSERT_EQUAL_STRING("Sensor fault: light sensor", firstReason(faulted(SkyRole::Sqm, false, false), t).c_str());
    TEST_ASSERT_EQUAL_STRING("Sensor fault: light sensor and IR sky sensor", firstReason(faulted(SkyRole::Sqm, false, true), t).c_str());
    // SQM authoritative with the SQM-L on.
    TEST_ASSERT_EQUAL_STRING("Sensor fault: SQM sensor", firstReason(faulted(SkyRole::Sqm, true, false), t).c_str());
    TEST_ASSERT_EQUAL_STRING("Sensor fault: SQM sensor and IR sky sensor", firstReason(faulted(SkyRole::Sqm, true, true), t).c_str());
    // SQM-L authoritative (with or without the SQM on).
    TEST_ASSERT_EQUAL_STRING("Sensor fault: SQM-L sensor", firstReason(faulted(SkyRole::Sqml, true, false), t).c_str());
    TEST_ASSERT_EQUAL_STRING("Sensor fault: SQM-L sensor", firstReason(faulted(SkyRole::Sqml, false, false), t).c_str());
    TEST_ASSERT_EQUAL_STRING("Sensor fault: SQM-L sensor and IR sky sensor", firstReason(faulted(SkyRole::Sqml, true, true), t).c_str());

    // The IR sensor alone keeps its reason whatever the sky role.
    SafetyInputs ir = skyOk();
    ir.skyRole = SkyRole::Sqml;
    ir.skyNamed = true;
    ir.requiredSensorFault = true;
    ir.irSkyFault = true;
    TEST_ASSERT_EQUAL_STRING("Sensor fault: IR sky sensor", firstReason(ir, t).c_str());
}

void test_sky_limit_reasons_per_role()
{
    SafetyInputs in = skyOk();
    TEST_ASSERT_EQUAL_STRING("SQM 18.21 < 19.50", firstReason(in, sqmLimit()).c_str());
    in.skyNamed = true;
    TEST_ASSERT_EQUAL_STRING("SQM 18.21 < 19.50", firstReason(in, sqmLimit()).c_str());
    in.skyRole = SkyRole::Sqml;
    TEST_ASSERT_EQUAL_STRING("SQM-L 18.21 < 19.50", firstReason(in, sqmLimit()).c_str());

    // A faulted authoritative sensor: the limit rule is not evaluated.
    const SafetyResult r = evaluateSafety(faulted(SkyRole::Sqml, true, false), sqmLimit());
    TEST_ASSERT_FALSE(r.isSafe);
    TEST_ASSERT_TRUE(r.reasonFlags & UnsafeSensorFault);
    TEST_ASSERT_FALSE(r.reasonFlags & UnsafeSkyBright);
}
