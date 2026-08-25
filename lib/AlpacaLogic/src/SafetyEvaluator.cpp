#include "SafetyEvaluator.h"

namespace SQM
{
    namespace Alpaca
    {

        SafetyResult evaluateSafety(const SafetyInputs &in, const SafetyThresholds &t)
        {
            SafetyResult result;

            if (t.manualOverrideUnsafe)
            {
                result.unsafeReasons.push_back("Manual override forces unsafe");
            }

            if (!in.hasEverHadGoodData)
            {
                result.unsafeReasons.push_back("No successful sensor data yet");
            }
            else if (in.secondsSinceLastGoodData > t.staleAfterSeconds)
            {
                result.unsafeReasons.push_back("Sensor data is stale");
            }

            if (in.requiredSensorFault)
            {
                result.unsafeReasons.push_back("A required sensor is reporting a fault");
            }

            // Threshold checks only apply once we have fresh data - an unsafe
            // verdict from missing/stale data above already covers that case,
            // and comparing garbage/zeroed readings here would just produce
            // misleading extra reasons.
            const bool haveFreshData = in.hasEverHadGoodData && in.secondsSinceLastGoodData <= t.staleAfterSeconds;

            if (haveFreshData)
            {
                if (t.cloudCoverEnabled && in.cloudCoverPercent >= t.cloudCoverUnsafePercent)
                {
                    result.unsafeReasons.push_back("Cloud cover at or above unsafe threshold");
                }

                if (t.sqmMinEnabled && in.sqm < t.sqmMinSafe)
                {
                    result.unsafeReasons.push_back("Sky brightness (SQM) below minimum safe value");
                }

                if (t.humidityMaxEnabled && in.humidityPercent > t.humidityMaxSafe)
                {
                    result.unsafeReasons.push_back("Humidity above maximum safe value");
                }

                if (t.dewpointMarginEnabled && (in.temperatureC - in.dewpointC) < t.dewpointMarginMinC)
                {
                    result.unsafeReasons.push_back("Temperature-dewpoint margin below minimum");
                }
            }

            result.isSafe = result.unsafeReasons.empty();
            return result;
        }

    } // namespace Alpaca
} // namespace SQM
