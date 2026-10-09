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

            // Rain first and regardless of the other sensors' freshness - a
            // stale sky sensor must never hide the fact that it's raining.
            void checkRain(SafetyResult &result, const SafetyInputs &in, const SafetyThresholds &t)
            {
                if (!in.rainSensorEnabled)
                    return;
                if (t.rainUnsafeEnabled && in.raining)
                    addReason(result, UnsafeRain, "Rain detected");
                if (t.rainSensorRequired && !in.rainSensorHealthy)
                    addReason(result, UnsafeRainSensorFault, "Rain sensor offline, stale or reporting a lens fault");
            }

            void checkWind(SafetyResult &result, const SafetyInputs &in, const SafetyThresholds &t)
            {
                if (!t.windSpeedUnsafeEnabled && !t.windGustUnsafeEnabled)
                    return;
                if (!in.windSensorEnabled || !in.windSensorHealthy)
                {
                    // A wind limit you can't measure is a limit you can't trust.
                    addReason(result, UnsafeWindSensorFault, "Wind limit set but the anemometer is disabled or not reporting");
                    return;
                }
                if (t.windSpeedUnsafeEnabled && in.windSpeedMs >= t.windSpeedUnsafeMs)
                    addReasonf(result, UnsafeWind, "Wind %.1f m/s >= %.1f m/s", in.windSpeedMs, t.windSpeedUnsafeMs);
                if (t.windGustUnsafeEnabled && in.windGustMs >= t.windGustUnsafeMs)
                    addReasonf(result, UnsafeWindGust, "Gust %.1f m/s >= %.1f m/s", in.windGustMs, t.windGustUnsafeMs);
            }

            void checkFreshness(SafetyResult &result, const SafetyInputs &in, const SafetyThresholds &t)
            {
                if (!in.hasEverHadGoodData)
                {
                    addReason(result, UnsafeNoData, "No successful sensor data yet");
                }
                else if (in.secondsSinceLastGoodData > t.staleAfterSeconds)
                {
                    addReasonf(
                        result,
                        UnsafeStaleData,
                        "Sensor data is stale (%us old, limit %us)",
                        static_cast<unsigned>(in.secondsSinceLastGoodData),
                        static_cast<unsigned>(t.staleAfterSeconds));
                }
            }

            void checkSensorFaults(SafetyResult &result, const SafetyInputs &in)
            {
                if (!in.requiredSensorFault)
                    return;
                if (in.skyLightFault && in.irSkyFault)
                    addReason(result, UnsafeSensorFault, "Sensor fault: light sensor and IR sky sensor");
                else if (in.skyLightFault)
                    addReason(result, UnsafeSensorFault, "Sensor fault: light sensor");
                else if (in.irSkyFault)
                    addReason(result, UnsafeSensorFault, "Sensor fault: IR sky sensor");
                else
                    addReason(result, UnsafeSensorFault, "A required sensor is reporting a fault");
            }

            void checkSky(SafetyResult &result, const SafetyInputs &in, const SafetyThresholds &t)
            {
                if (t.cloudCoverEnabled && !in.irSkyFault && in.cloudCoverPercent >= t.cloudCoverUnsafePercent)
                    addReasonf(result, UnsafeCloudCover, "Cloud %.0f%% >= %.0f%%", in.cloudCoverPercent, t.cloudCoverUnsafePercent);
                if (t.sqmMinEnabled && !in.skyLightFault && in.sqm < t.sqmMinSafe)
                    addReasonf(result, UnsafeSkyBright, "SQM %.2f < %.2f", in.sqm, t.sqmMinSafe);
            }

            void checkEnvironment(SafetyResult &result, const SafetyInputs &in, const SafetyThresholds &t)
            {
                const bool environmentRulesEnabled = t.humidityMaxEnabled || t.dewpointMarginEnabled;
                if (environmentRulesEnabled && in.environmentSensorFault)
                {
                    addReason(result, UnsafeEnvironmentFault, "Sensor fault: environment sensor");
                    return;
                }
                if (t.humidityMaxEnabled && in.humidityPercent > t.humidityMaxSafe)
                    addReasonf(result, UnsafeHumidity, "Humidity %.0f%% > %.0f%%", in.humidityPercent, t.humidityMaxSafe);
                const float margin = in.temperatureC - in.dewpointC;
                if (t.dewpointMarginEnabled && margin < t.dewpointMarginMinC)
                {
                    addReasonf(
                        result,
                        UnsafeDewpoint,
                        "Dew margin %.1f C < %.1f C (temp %.1f C, dew point %.1f C)",
                        in.temperatureC - in.dewpointC,
                        t.dewpointMarginMinC,
                        in.temperatureC,
                        in.dewpointC);
                }
            }
        } // namespace

        SafetyResult evaluateSafety(const SafetyInputs &in, const SafetyThresholds &t)
        {
            SafetyResult result;
            if (t.manualOverrideUnsafe)
                addReason(result, UnsafeManualOverride, "Manual override forces unsafe");
            checkRain(result, in, t);
            checkWind(result, in, t);
            checkFreshness(result, in, t);
            checkSensorFaults(result, in);

            // Threshold checks only apply once we have fresh data - an unsafe
            // verdict from missing/stale data above already covers that case,
            // and comparing garbage/zeroed readings here would just produce
            // misleading extra reasons.
            const bool haveFreshData = in.hasEverHadGoodData && in.secondsSinceLastGoodData <= t.staleAfterSeconds;
            if (haveFreshData)
            {
                checkSky(result, in, t);
                checkEnvironment(result, in, t);
            }

            result.isSafe = result.unsafeReasons.empty();
            return result;
        }

        bool SafeDelayFilter::update(bool rawSafe, uint32_t nowMs, uint32_t delaySeconds)
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
                safeSinceMs = nowMs;
            }
            const uint32_t elapsed = (nowMs - safeSinceMs) / 1000;
            remaining = elapsed >= delaySeconds ? 0 : delaySeconds - elapsed;
            return remaining == 0;
        }

    } // namespace Alpaca
} // namespace SQM
