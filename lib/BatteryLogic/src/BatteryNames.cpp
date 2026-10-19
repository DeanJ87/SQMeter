#include "BatteryLogic.h"

namespace SQM
{
    namespace Battery
    {
        const char *chargeStateName(ChargeState state)
        {
            switch (state)
            {
            case ChargeState::Discharging:
                return "discharging";
            case ChargeState::Charging:
                return "charging";
            case ChargeState::Charged:
                return "charged";
            case ChargeState::Fault:
                return "fault";
            case ChargeState::NoBattery:
                return "no-battery";
            case ChargeState::Unknown:
                break;
            }
            return nullptr;
        }

        const char *chargeFaultName(ChargeFault fault)
        {
            return fault == ChargeFault::Temperature ? "temperature" : nullptr;
        }

        const char *powerStateName(PowerState state)
        {
            switch (state)
            {
            case PowerState::Saving:
                return "saving";
            case PowerState::ShutdownPending:
                return "shutdown";
            case PowerState::Normal:
                break;
            }
            return "normal";
        }
    } // namespace Battery
} // namespace SQM
