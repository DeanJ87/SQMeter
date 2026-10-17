#pragma once

// Li-ion battery logic (specs/032-battery): MAX17048 fuel gauge decoding, the
// MCP73871 charger's status lines, time estimates and the power policy. Pure:
// no Arduino headers, so the firmware, the native tests and the browser demo
// share it.

#include <cstddef>
#include <cstdint>

namespace SQM
{
    namespace Battery
    {
        // What the charger (or, without its pins, the gauge's rate) says.
        enum class ChargeState : uint8_t
        {
            Discharging,
            Charging,
            Charged,
            Fault,
            NoBattery,
            Unknown, // nothing known yet (no charger pins and no rate)
        };

        // JSON name ("discharging", "charging", "charged", "fault",
        // "no-battery"); nullptr for Unknown.
        const char *chargeStateName(ChargeState state);

        enum class ChargeFault : uint8_t
        {
            None,
            Temperature, // the charger's NTC window (the safety timer is off on Rev A)
        };

        // "temperature", or nullptr for None.
        const char *chargeFaultName(ChargeFault fault);

        // research D7: Normal -> Saving -> ShutdownPending -> deep sleep.
        enum class PowerState : uint8_t
        {
            Normal,
            Saving,
            ShutdownPending,
        };

        // "normal", "saving" or "shutdown" (status diagnostics).
        const char *powerStateName(PowerState state);
    } // namespace Battery
} // namespace SQM
