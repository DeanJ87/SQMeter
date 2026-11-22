#include "AlertEngine.h"

#include <cstdio>

// Rain, the rain sensor's lens and dew suspected on it (spec 036). The rest of
// the engine is in AlertEngine.cpp.

namespace SQM
{
    namespace Alerts
    {
        namespace
        {
            // Below this the rate prints as 0.00 mm/h: the RG-15 latches rain
            // on its first drops (accumulation) before it reports a rate.
            constexpr float RAIN_RATE_SHOWN_MM_PER_HOUR = 0.005f;

            Alert make(AlertType type, std::string title, std::string message)
            {
                Alert alert;
                alert.type = type;
                alert.title = std::move(title);
                alert.message = std::move(message);
                return alert;
            }

            std::string format(const char *fmt, double a, double b = 0.0)
            {
                char buffer[160];
                std::snprintf(buffer, sizeof(buffer), fmt, a, b);
                return buffer;
            }
        } // namespace

        void AlertEngine::updateRain(const AlertInputs &in, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts)
        {
            // Rain
            if (in.rainEnabled)
            {
                if (sync(rain, in.raining, step.emit(rules.onRain)))
                {
                    if (in.raining)
                    {
                        // Sent at once, with or without a rate yet: rain is the
                        // most urgent alert.
                        const bool firstDrops = in.rainRateMmPerHour < RAIN_RATE_SHOWN_MM_PER_HOUR;
                        alerts.push_back(
                            firstDrops ? make(AlertType::RainStarted, "Rain detected", "The rain sensor reports the first drops of rain.")
                                       : make(
                                             AlertType::RainStarted,
                                             "Rain detected",
                                             format("The rain sensor reports rain (%.2f mm/h).", in.rainRateMmPerHour)));
                        // {rain_rate} stays a number ("0.00" on the first drops)
                        // so custom templates still read.
                        alerts.back().vars = {{"rain_rate", format("%.2f", firstDrops ? 0.0 : in.rainRateMmPerHour)}};
                    }
                    else
                        alerts.push_back(make(AlertType::RainStopped, "Rain cleared", "No rain for the configured rain clear delay."));
                }

                if (sync(lens, in.lensFault, step.emit(rules.onSensorFault, rules.sensorFaultDelaySeconds)) && in.lensFault)
                {
                    alerts.push_back(make(
                        AlertType::LensFault,
                        "Rain sensor lens fault",
                        "The rain sensor reports a lens fault. Clean or inspect the lens."));
                    alerts.back().vars = {{"sensor", "RG-15 lens"}};
                }

                // Dew suspected (spec 036): waits like a lens fault, so a
                // passing shower that cloud confirms a moment later doesn't page.
                if (sync(dewSuspected, in.dewSuspected, step.emit(rules.onDewSuspected, rules.sensorFaultDelaySeconds)) && in.dewSuspected)
                    alerts.push_back(make(
                        AlertType::DewSuspected,
                        "Rain sensor: dew suspected",
                        format(
                            "Rain %.2f mm/h under a clear sky (cloud %.0f%%). Check the rain sensor's lens.",
                            in.rainRateMmPerHour,
                            in.dewCloudCoverPercent)));
            }
        }

    } // namespace Alerts
} // namespace SQM
