#include "BatteryLogic.h"

#include <algorithm>
#include <cmath>

// Time to empty or full from the charge and its smoothed rate (research D4).

namespace SQM
{
    namespace Battery
    {
        namespace
        {
            uint32_t minutes(float hours)
            {
                const float value = std::round(hours * 60.0f);
                return value >= static_cast<float>(MAX_ESTIMATE_MINUTES) ? MAX_ESTIMATE_MINUTES : static_cast<uint32_t>(value);
            }
        } // namespace

        Estimate estimate(const EstimateInput &in)
        {
            Estimate out;
            if (!in.valid || !in.rateValid)
                return out;
            if (in.capacityMah != 0)
                out.currentMa = in.rate / 100.0f * static_cast<float>(in.capacityMah);
            if (std::fabs(in.rate) < MIN_ESTIMATE_RATE)
                return out;
            if (in.state == ChargeState::Discharging && in.rate < 0.0f)
                out.timeToEmptyMinutes = minutes(in.percent / -in.rate);
            else if (in.state == ChargeState::Charging && in.rate > 0.0f)
            {
                float hours = (100.0f - in.percent) / in.rate;
                // The charger can't go faster than its set current.
                if (in.capacityMah != 0)
                    hours = std::max(hours, (100.0f - in.percent) / 100.0f * static_cast<float>(in.capacityMah) / CHARGE_CURRENT_MA);
                out.timeToFullMinutes = minutes(hours);
            }
            return out;
        }
    } // namespace Battery
} // namespace SQM
