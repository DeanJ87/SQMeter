#include "SafetyEvaluator.h"

#include <cstdarg>
#include <cstdio>

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

            // Reasons say what was measured and what the limit is, e.g.
            // "SQM 18.21 < 19.50", so an alert explains itself.
            void addReasonf(SafetyResult &result, UnsafeReasonFlag flag, const char *format, ...)
            {
                char buffer[96];
                va_list args;
                va_start(args, format);
                vsnprintf(buffer, sizeof(buffer), format, args);
                va_end(args);
                addReason(result, flag, buffer);
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

            const bool windRulesEnabled = t.windSpeedUnsafeEnabled || t.windGustUnsafeEnabled;
            if (windRulesEnabled)
            {
                if (!in.windSensorEnabled || !in.windSensorHealthy)
                {
                    // A wind limit you can't measure is a limit you can't trust.
                    addReason(result, UNSAFE_WIND_SENSOR_FAULT, "Wind limit set but the anemometer is disabled or not reporting");
                }
                else
                {
                    if (t.windSpeedUnsafeEnabled && in.windSpeedMs >= t.windSpeedUnsafeMs)
                        addReasonf(result, UNSAFE_WIND, "Wind %.1f m/s >= %.1f m/s", in.windSpeedMs, t.windSpeedUnsafeMs);
                    if (t.windGustUnsafeEnabled && in.windGustMs >= t.windGustUnsafeMs)
                        addReasonf(result, UNSAFE_WIND_GUST, "Gust %.1f m/s >= %.1f m/s", in.windGustMs, t.windGustUnsafeMs);
                }
            }

            if (!in.hasEverHadGoodData)
            {
                addReason(result, UNSAFE_NO_DATA, "No successful sensor data yet");
            }
            else if (in.secondsSinceLastGoodData > t.staleAfterSeconds)
            {
                addReasonf(result, UNSAFE_STALE_DATA, "Sensor data is stale (%us old, limit %us)",
                           static_cast<unsigned>(in.secondsSinceLastGoodData), static_cast<unsigned>(t.staleAfterSeconds));
            }

            if (in.requiredSensorFault)
            {
                if (in.skyLightFault && in.irSkyFault)
                    addReason(result, UNSAFE_SENSOR_FAULT, "Sensor fault: TSL2591 light and MLX90614 IR");
                else if (in.skyLightFault)
                    addReason(result, UNSAFE_SENSOR_FAULT, "Sensor fault: TSL2591 light");
                else if (in.irSkyFault)
                    addReason(result, UNSAFE_SENSOR_FAULT, "Sensor fault: MLX90614 IR");
                else
                    addReason(result, UNSAFE_SENSOR_FAULT, "A required sensor is reporting a fault");
            }

            // Threshold checks only apply once we have fresh data - an unsafe
            // verdict from missing/stale data above already covers that case,
            // and comparing garbage/zeroed readings here would just produce
            // misleading extra reasons.
            const bool haveFreshData = in.hasEverHadGoodData && in.secondsSinceLastGoodData <= t.staleAfterSeconds;

            if (haveFreshData)
            {
                if (t.cloudCoverEnabled && !in.irSkyFault && in.cloudCoverPercent >= t.cloudCoverUnsafePercent)
                {
                    addReasonf(result, UNSAFE_CLOUD_COVER, "Cloud %.0f%% >= %.0f%%", in.cloudCoverPercent, t.cloudCoverUnsafePercent);
                }

                if (t.sqmMinEnabled && !in.skyLightFault && in.sqm < t.sqmMinSafe)
                {
                    addReasonf(result, UNSAFE_SKY_BRIGHT, "SQM %.2f < %.2f", in.sqm, t.sqmMinSafe);
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
                        addReasonf(result, UNSAFE_HUMIDITY, "Humidity %.0f%% > %.0f%%", in.humidityPercent, t.humidityMaxSafe);
                    }

                    if (t.dewpointMarginEnabled && (in.temperatureC - in.dewpointC) < t.dewpointMarginMinC)
                    {
                        addReasonf(result, UNSAFE_DEWPOINT, "Dew margin %.1f C < %.1f C (temp %.1f C, dew point %.1f C)",
                                   in.temperatureC - in.dewpointC, t.dewpointMarginMinC, in.temperatureC, in.dewpointC);
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
