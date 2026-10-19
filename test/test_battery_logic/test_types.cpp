#include <unity.h>

#include "BatteryLogic.h"
#include "SensorTypes.h"

// Battery names and the reading's defaults (specs/032-battery, data-model.md).

using namespace SQM;
using namespace SQM::Battery;

void test_charge_state_names(void)
{
    TEST_ASSERT_EQUAL_STRING("discharging", chargeStateName(ChargeState::Discharging));
    TEST_ASSERT_EQUAL_STRING("charging", chargeStateName(ChargeState::Charging));
    TEST_ASSERT_EQUAL_STRING("charged", chargeStateName(ChargeState::Charged));
    TEST_ASSERT_EQUAL_STRING("fault", chargeStateName(ChargeState::Fault));
    TEST_ASSERT_EQUAL_STRING("no-battery", chargeStateName(ChargeState::NoBattery));
    TEST_ASSERT_NULL(chargeStateName(ChargeState::Unknown));
}

void test_fault_and_power_names(void)
{
    TEST_ASSERT_EQUAL_STRING("temperature", chargeFaultName(ChargeFault::Temperature));
    TEST_ASSERT_NULL(chargeFaultName(ChargeFault::None));
    TEST_ASSERT_EQUAL_STRING("normal", powerStateName(PowerState::Normal));
    TEST_ASSERT_EQUAL_STRING("saving", powerStateName(PowerState::Saving));
    TEST_ASSERT_EQUAL_STRING("shutdown", powerStateName(PowerState::ShutdownPending));
}

void test_battery_reading_defaults(void)
{
    const BatteryReading r;
    TEST_ASSERT_TRUE(r.status == SensorStatus::NotInitialized);
    TEST_ASSERT_EQUAL_UINT32(0, r.timestamp);
    TEST_ASSERT_TRUE(r.estimating);
    TEST_ASSERT_FALSE(r.rateValid);
    TEST_ASSERT_FALSE(r.chargerPins);
    TEST_ASSERT_TRUE(r.chargeState == ChargeState::Unknown);
    TEST_ASSERT_FALSE(r.usbPower.has_value());
    TEST_ASSERT_FALSE(r.gaugeAlert.has_value());
    TEST_ASSERT_TRUE(r.fault == ChargeFault::None);
}

void run_types_tests(void)
{
    RUN_TEST(test_charge_state_names);
    RUN_TEST(test_fault_and_power_names);
    RUN_TEST(test_battery_reading_defaults);
}
