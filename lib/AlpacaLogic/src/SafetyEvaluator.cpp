#include "SafetyEvaluator.h"

namespace SQM
{
    namespace Alpaca
    {

        namespace
        {
            void addReason(SafetyResult &result, UnsafeReasonFlag flag, const char *reason)
            {
                result.reasonFlags |= flag;
                result.unsafeReasons.push_back(reason);
            }
        }

        SafetyResult evaluateSafety(const SafetyInputs &in, const SafetyThresholds &t)
        {
            SafetyResult result;

            if (t.manualOverrideUnsafe)
            {
                addReason(result, UNSAFE_MANUAL_OVERRIDE, "Manual override forces unsafe");
            }

            // Rain first and regardless of the other sensors' freshness - a
            // stale sky sensor must never hide the fact that it's raining.
            if (in.rainSensorEnabled)
            {
                if (t.rainUnsafeEnabled && in.raining)
                {
                    addReason(result, UNSAFE_RAIN, "Rain detected");
                }
                if (t.rainSensorRequired && !in.rainSensorHealthy)
                {
                    addReason(result, UNSAFE_RAIN_SENSOR_FAULT, "Rain sensor offline, stale or reporting a lens fault");
                }
            }

            if (!in.hasEverHadGoodData)
            {
                addReason(result, UNSAFE_NO_DATA, "No successful sensor data yet");
            }
            else if (in.secondsSinceLastGoodData > t.staleAfterSeconds)
            {
                addReason(result, UNSAFE_STALE_DATA, "Sensor data is stale");
            }

            if (in.requiredSensorFault)
            {
                addReason(result, UNSAFE_SENSOR_FAULT, "A required sensor is reporting a fault");
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
                    addReason(result, UNSAFE_CLOUD_COVER, "Cloud cover at or above unsafe threshold");
                }

                if (t.sqmMinEnabled && in.sqm < t.sqmMinSafe)
                {
                    addReason(result, UNSAFE_SKY_BRIGHT, "Sky brightness (SQM) below minimum safe value");
                }

                const bool environmentRulesEnabled = t.humidityMaxEnabled || t.dewpointMarginEnabled;
                if (environmentRulesEnabled && in.environmentSensorFault)
                {
                    addReason(result, UNSAFE_ENVIRONMENT_FAULT, "Humidity sensor fault - humidity/dew point rules can't be evaluated");
                }
                else
                {
                    if (t.humidityMaxEnabled && in.humidityPercent > t.humidityMaxSafe)
                    {
                        addReason(result, UNSAFE_HUMIDITY, "Humidity above maximum safe value");
                    }

                    if (t.dewpointMarginEnabled && (in.temperatureC - in.dewpointC) < t.dewpointMarginMinC)
                    {
                        addReason(result, UNSAFE_DEWPOINT, "Temperature-dewpoint margin below minimum");
                    }
                }
            }

            result.isSafe = result.unsafeReasons.empty();
            return result;
        }

        bool SafeDelayFilter::update(bool rawSafe, uint32_t nowSeconds, uint32_t delaySeconds)
        {
            if (!rawSafe)
            {
                rawSafeRunning = false;
                remaining = 0;
                return false;
            }
            if (!rawSafeRunning)
            {
                rawSafeRunning = true;
                safeSince = nowSeconds;
            }
            const uint32_t elapsed = nowSeconds - safeSince;
            remaining = elapsed >= delaySeconds ? 0 : delaySeconds - elapsed;
            return remaining == 0;
        }

    } // namespace Alpaca
} // namespace SQM
