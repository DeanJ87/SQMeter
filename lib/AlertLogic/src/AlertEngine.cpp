#include "AlertEngine.h"

#include <cstdio>

namespace SQM
{
    namespace Alerts
    {

        namespace
        {
            // Hysteresis bands so a reading hovering on a threshold doesn't
            // re-trigger the same condition.
            constexpr float DEW_HYSTERESIS_C = 0.5f;
            constexpr float CLEAR_HYSTERESIS_PERCENT = 10.0f;

            Alert make(AlertType type, AlertPriority priority, std::string title, std::string message)
            {
                Alert alert;
                alert.type = type;
                alert.priority = priority;
                alert.title = std::move(title);
                alert.message = std::move(message);
                return alert;
            }

            std::string format(const char *fmt, double a, double b = 0.0, double c = 0.0)
            {
                char buffer[160];
                std::snprintf(buffer, sizeof(buffer), fmt, a, b, c);
                return buffer;
            }
        }

        const char *alertTypeName(AlertType type)
        {
            switch (type)
            {
            case AlertType::Unsafe:
                return "unsafe";
            case AlertType::Safe:
                return "safe";
            case AlertType::RainStarted:
                return "rain_started";
            case AlertType::RainStopped:
                return "rain_stopped";
            case AlertType::SensorFault:
                return "sensor_fault";
            case AlertType::SensorRecovered:
                return "sensor_recovered";
            case AlertType::LensFault:
                return "lens_fault";
            case AlertType::DewRisk:
                return "dew_risk";
            case AlertType::ClearSky:
                return "clear_sky";
            case AlertType::Acknowledged:
                return "acknowledged";
            case AlertType::Test:
                return "test";
            }
            return "unknown";
        }

        std::string joinReasons(const std::vector<std::string> &reasons)
        {
            std::string out;
            for (const std::string &reason : reasons)
            {
                if (!out.empty())
                    out += "; ";
                out += reason;
            }
            return out;
        }

        bool AlertEngine::sync(Tracker &tracker, bool current, uint32_t now, uint32_t cooldown, bool emitAllowed)
        {
            if (!tracker.initialized || !emitAllowed)
            {
                // Follow the condition silently so enabling a rule (or the end
                // of the startup grace) doesn't announce a stale transition.
                tracker.initialized = true;
                tracker.notified = current;
                return false;
            }
            if (current == tracker.notified)
                return false;
            if (tracker.hasNotified && now - tracker.lastNotifiedAt < cooldown)
                return false; // retried on a later update
            tracker.notified = current;
            tracker.hasNotified = true;
            tracker.lastNotifiedAt = now;
            return true;
        }

        std::vector<Alert> AlertEngine::update(const AlertInputs &in, const AlertRules &rules)
        {
            std::vector<Alert> alerts;
            if (!started)
            {
                started = true;
                startedAt = in.nowSeconds;
            }
            const bool pastGrace = in.nowSeconds - startedAt >= rules.startupGraceSeconds;
            const uint32_t now = in.nowSeconds;
            const uint32_t cooldown = rules.cooldownSeconds;

            // Safety verdict
            if (in.safetyKnown)
            {
                const bool unsafe = !in.isSafe;
                if (sync(safety, unsafe, now, cooldown, pastGrace && rules.onSafetyChange))
                {
                    if (unsafe)
                        alerts.push_back(make(AlertType::Unsafe, AlertPriority::High, "Observatory UNSAFE",
                                              in.unsafeReasons.empty() ? std::string("Safety rules failing") : joinReasons(in.unsafeReasons)));
                    else
                        alerts.push_back(make(AlertType::Safe, AlertPriority::Normal, "Observatory safe",
                                              "All enabled safety rules pass."));
                }
            }

            // Rain
            if (in.rainEnabled)
            {
                if (sync(rain, in.raining, now, cooldown, pastGrace && rules.onRain))
                {
                    if (in.raining)
                        alerts.push_back(make(AlertType::RainStarted, AlertPriority::High, "Rain detected",
                                              format("The rain sensor reports rain (%.1f mm/h).", in.rainRateMmPerHour)));
                    else
                        alerts.push_back(make(AlertType::RainStopped, AlertPriority::Normal, "Rain cleared",
                                              "No rain for the configured rain clear delay."));
                }

                if (sync(lens, in.lensFault, now, cooldown, pastGrace && rules.onSensorFault) && in.lensFault)
                {
                    alerts.push_back(make(AlertType::LensFault, AlertPriority::Normal, "Rain sensor lens fault",
                                          "The RG-15 reports a lens fault - clean or inspect the lens."));
                }
            }

            // Sensor health
            for (size_t i = 0; i < SENSOR_COUNT; ++i)
            {
                const SensorHealth &sensor = in.sensors[i];
                if (!sensor.enabled)
                {
                    sensors[i] = Tracker{};
                    continue;
                }
                const bool faulted = !sensor.healthy;
                if (sync(sensors[i], faulted, now, cooldown, pastGrace && rules.onSensorFault))
                {
                    if (faulted)
                        alerts.push_back(make(AlertType::SensorFault, AlertPriority::High, std::string(sensor.name) + " sensor fault",
                                              std::string(sensor.name) + " is offline or reporting errors."));
                    else
                        alerts.push_back(make(AlertType::SensorRecovered, AlertPriority::Normal, std::string(sensor.name) + " sensor recovered",
                                              std::string(sensor.name) + " is reporting normally again."));
                }
            }

            // Dew risk (with hysteresis), only alert on onset
            if (in.environmentValid)
            {
                const float margin = in.temperatureC - in.dewpointC;
                dewObserved = dewObserved ? margin < rules.dewRiskMarginC + DEW_HYSTERESIS_C
                                          : margin < rules.dewRiskMarginC;
                if (sync(dew, dewObserved, now, cooldown, pastGrace && rules.onDewRisk) && dewObserved)
                {
                    alerts.push_back(make(AlertType::DewRisk, AlertPriority::Normal, "Dew risk",
                                          format("Temperature %.1f C is within %.1f C of the dew point (%.1f C).",
                                                 in.temperatureC, margin, in.dewpointC)));
                }
            }

            // Clear sky (with hysteresis), only alert on onset
            if (in.skyValid)
            {
                clearObserved = clearObserved ? in.cloudCoverPercent < rules.clearSkyCloudPercent + CLEAR_HYSTERESIS_PERCENT
                                              : in.cloudCoverPercent < rules.clearSkyCloudPercent;
                if (sync(clear, clearObserved, now, cooldown, pastGrace && rules.onClearSky) && clearObserved)
                {
                    alerts.push_back(make(AlertType::ClearSky, AlertPriority::Normal, "Clear skies",
                                          format("Cloud cover has dropped to %.0f%%.", in.cloudCoverPercent)));
                }
            }

            return alerts;
        }

    } // namespace Alerts
} // namespace SQM
