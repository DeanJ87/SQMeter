#include "DeviceCoreInternal.h"

// The battery (specs/032-battery): presence, estimates and what every
// interface reports.

namespace SQM::Core
{
    namespace Detail
    {
        void deriveBattery(BatterySnapshot &battery, const Config &cfg)
        {
            battery.enabled = cfg.battery.enabled;
            // Monitoring off: reported as if no gauge were fitted (research D8).
            battery.present = battery.enabled && (battery.detected || cfg.battery.chargerPins());
        }
    } // namespace Detail
} // namespace SQM::Core
