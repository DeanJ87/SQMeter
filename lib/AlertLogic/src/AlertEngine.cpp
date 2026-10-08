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

            Alert make(AlertType type, std::string title, std::string message)
            {
                Alert alert;
                alert.type = type;
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

        const char *alertLevelName(AlertLevel level)
        {
            switch (level)
            {
            case AlertLevel::Quiet:
                return "quiet";
            case AlertLevel::Normal:
                return "normal";
            case AlertLevel::Urgent:
                return "urgent";
            case AlertLevel::Wake:
                return "wake";
            default:
                return "off";
            }
        }

        int pushoverPriority(AlertLevel level)
        {
            switch (level)
            {
            case AlertLevel::Quiet:
                return -1;
            case AlertLevel::Urgent:
                return 1;
            case AlertLevel::Wake:
                return 2;
            case AlertLevel::Normal:
            default:
                return 0;
            }
        }

        const char *ntfyPriority(AlertLevel level)
        {
            switch (level)
            {
            case AlertLevel::Quiet:
                return "low";
            case AlertLevel::Urgent:
                return "high";
            case AlertLevel::Wake:
                return "max";
            case AlertLevel::Normal:
            default:
                return "default";
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
            case AlertType::CloudedOver:
                return "clouded_over";
            case AlertType::Acknowledged:
                return "acknowledged";
            case AlertType::AlertsOn:
                return "alerts_on";
            case AlertType::Test:
                return "test";
            case AlertType::ClientLost:
                return "client_lost";
            case AlertType::ClientBack:
                return "client_back";
            case AlertType::ClientDisconnected:
                return "client_disconnected";
            }
            return "unknown";
        }

        std::string joinReasons(const std::vector<std::string> &reasons)
        {
            std::string out;
            for (const std::string &reason : reasons)
            {
                if (!out.empty())
                    out += "\n";
                out += "\u2022 " + reason;
            }
            return out;
        }

        std::string renderTemplate(const std::string &text, const std::vector<std::pair<std::string, std::string>> &vars)
        {
            std::string out;
            out.reserve(text.size() + 32);
            size_t i = 0;
            while (i < text.size())
            {
                const size_t open = text.find('{', i);
                if (open == std::string::npos)
                {
                    out.append(text, i, std::string::npos);
                    break;
                }
                out.append(text, i, open - i);
                const size_t close = text.find('}', open + 1);
                if (close == std::string::npos)
                {
                    out.append(text, open, std::string::npos);
                    break;
                }
                const std::string name = text.substr(open + 1, close - open - 1);
                const std::string *value = nullptr;
                for (const auto &var : vars)
                    if (var.first == name)
                        value = &var.second;
                if (value != nullptr)
                    out += *value;
                else
                    out.append(text, open, close - open + 1);
                i = close + 1;
            }
            return out;
        }

        Alert stackAlerts(const std::vector<Alert> &alerts)
        {
            if (alerts.empty())
                return Alert{};
            size_t lead = 0;
            for (size_t i = 1; i < alerts.size(); ++i)
                if (alerts[i].level > alerts[lead].level)
                    lead = i;
            Alert stacked = alerts[lead];
            if (alerts.size() == 1)
                return stacked;
            stacked.title.clear();
            stacked.message.clear();
            // Lead first, the rest in the order they were raised.
            std::vector<size_t> order{lead};
            for (size_t i = 0; i < alerts.size(); ++i)
                if (i != lead)
                {
                    order.push_back(i);
                    stacked.stacked.push_back(alerts[i].type);
                }
            for (size_t i : order)
            {
                if (!stacked.title.empty())
                    stacked.title += " \u00b7 ";
                stacked.title += alerts[i].title;
                if (!stacked.message.empty())
                    stacked.message += "\n\n";
                stacked.message += alerts[i].message;
            }
            return stacked;
        }

        bool AlertEngine::sync(Tracker &tracker, bool current, uint32_t now, uint32_t cooldown, bool emitAllowed, uint32_t settle)
        {
            if (!tracker.initialized || !emitAllowed)
            {
                // Follow the condition silently so enabling a rule (or the end
                // of the startup grace) doesn't announce a stale transition.
                tracker.initialized = true;
                tracker.notified = current;
                tracker.pending = false;
                return false;
            }
            if (current == tracker.notified)
            {
                tracker.pending = false;
                return false;
            }
            if (!tracker.pending)
            {
                tracker.pending = true;
                tracker.pendingSince = now;
            }
            if (now - tracker.pendingSince < settle)
                return false;
            if (tracker.hasNotified && now - tracker.lastNotifiedAt < cooldown)
                return false; // retried on a later update
            tracker.notified = current;
            tracker.pending = false;
            tracker.hasNotified = true;
            tracker.lastNotifiedAt = now;
            return true;
        }

        void AlertEngine::seedSafety(bool unsafe)
        {
            safety = Tracker{};
            safety.initialized = true;
            safety.notified = unsafe;
            safetySeeded = true;
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
            // A seeded verdict waits out the startup grace instead of being
            // overwritten by whatever the sensors say while starting up.
            const bool holdSeed = safetySeeded && !pastGrace;
            const bool safetyDaylight = rules.safetyNightOnly && in.nightKnown && !in.isNight;
            if (in.safetyKnown && !in.safetySettling && !holdSeed && !safetyDaylight)
            {
                const bool unsafe = !in.isSafe;
                if (sync(safety, unsafe, now, cooldown, pastGrace && rules.onSafetyChange))
                {
                    if (unsafe)
                    {
                        const std::string reasons = in.unsafeReasons.empty() ? std::string("Safety rules failing") : joinReasons(in.unsafeReasons);
                        alerts.push_back(make(AlertType::Unsafe, "Observatory UNSAFE", reasons));
                        std::string inline_;
                        for (const std::string &reason : in.unsafeReasons)
                            inline_ += (inline_.empty() ? "" : "; ") + reason;
                        alerts.back().vars = {{"reasons", reasons},
                                              {"reasons_inline", inline_},
                                              {"reason_count", std::to_string(in.unsafeReasons.size())}};
                    }
                    else
                        alerts.push_back(make(AlertType::Safe, "Observatory safe",
                                              "All enabled safety rules pass."));
                }
            }

            // Rain
            if (in.rainEnabled)
            {
                if (sync(rain, in.raining, now, cooldown, pastGrace && rules.onRain))
                {
                    if (in.raining)
                    {
                        alerts.push_back(make(AlertType::RainStarted, "Rain detected",
                                              format("The rain sensor reports rain (%.1f mm/h).", in.rainRateMmPerHour)));
                        alerts.back().vars = {{"rain_rate", format("%.1f", in.rainRateMmPerHour)}};
                    }
                    else
                        alerts.push_back(make(AlertType::RainStopped, "Rain cleared",
                                              "No rain for the configured rain clear delay."));
                }

                if (sync(lens, in.lensFault, now, cooldown, pastGrace && rules.onSensorFault, rules.sensorSettleSeconds) && in.lensFault)
                {
                    alerts.push_back(make(AlertType::LensFault, "Rain sensor lens fault",
                                          "The RG-15 reports a lens fault - clean or inspect the lens."));
                    alerts.back().vars = {{"sensor", "RG-15 lens"}};
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
                if (sync(sensors[i], faulted, now, cooldown, pastGrace && rules.onSensorFault, rules.sensorSettleSeconds))
                {
                    if (faulted)
                        alerts.push_back(make(AlertType::SensorFault, std::string(sensor.name) + " sensor fault",
                                              std::string(sensor.name) + " is offline or reporting errors."));
                    else
                        alerts.push_back(make(AlertType::SensorRecovered, std::string(sensor.name) + " sensor recovered",
                                              std::string(sensor.name) + " is reporting normally again."));
                    alerts.back().vars = {{"sensor", sensor.name}};
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
                    alerts.push_back(make(AlertType::DewRisk, "Dew risk",
                                          format("Temperature %.1f °C is within %.1f °C of the dew point (%.1f °C).",
                                                 in.temperatureC, margin, in.dewpointC)));
                    alerts.back().vars = {{"dew_margin", format("%.1f", margin)}, {"dew_margin_min", format("%.1f", rules.dewRiskMarginC)}};
                }
            }

            // Sky clear / clouded over
            if (in.skyValid)
            {
                if (in.cloudCoverPercent < rules.clearSkyCloudPercent)
                    skyClear = true;
                else if (in.cloudCoverPercent > rules.cloudedOverCloudPercent)
                    skyClear = false;

                const bool dark = !rules.skyNightOnly || !in.nightKnown || in.isNight;
                if (!dark)
                {
                    // Daylight: hold everything, and treat the sky as not
                    // clear so a clear sky at nightfall is announced.
                    sky = Tracker{};
                    sky.initialized = true;
                    sky.notified = false;
                }
                else if (sync(sky, skyClear, now, cooldown, pastGrace && (rules.onClearSky || rules.onCloudedOver), rules.skySettleSeconds))
                {
                    if (skyClear && rules.onClearSky)
                        alerts.push_back(make(AlertType::ClearSky, rules.skyNightOnly ? "Dark and clear" : "Skies clear",
                                              format("Cloud cover is down to %.0f%% (clear below %.0f%%).", in.cloudCoverPercent, rules.clearSkyCloudPercent)));
                    else if (!skyClear && rules.onCloudedOver)
                        alerts.push_back(make(AlertType::CloudedOver, "Clouded over",
                                              format("Cloud cover is up to %.0f%% (cloudy above %.0f%%).", in.cloudCoverPercent, rules.cloudedOverCloudPercent)));
                }
            }

            updateClients(in, rules, alerts);
            return alerts;
        }

        void AlertEngine::updateClients(const AlertInputs &in, const AlertRules &rules, std::vector<Alert> &alerts)
        {
            const uint32_t now = in.nowSeconds;
            const uint32_t cooldown = rules.cooldownSeconds;
            for (size_t i = 0; i < CLIENT_DEVICE_COUNT; ++i)
            {
                const ClientInputs &client = in.clients[i];
                const std::string device = client.device;
                auto withVars = [&client, &device](Alert alert)
                {
                    // {device} is the Alpaca device here; it overrides the
                    // device name, as an event's own values do.
                    alert.vars = {{"device", device}, {"silent_for", client.silentFor}, {"last_checked", client.lastChecked}, {"client_id", client.clientId}};
                    return alert;
                };

                if (client.disconnectedNow)
                {
                    // A normal end of session: no "stopped checking", and no
                    // "is back" for the request that disconnected.
                    clients[i].initialized = true;
                    clients[i].notified = false;
                    clients[i].pending = false;
                    disconnectPending[i] = rules.onClientDisconnected;
                }
                else if (sync(clients[i], client.silent, now, cooldown, client.watching && (rules.onClientLost || rules.onClientBack)))
                {
                    if (client.silent && rules.onClientLost)
                        alerts.push_back(withVars(make(AlertType::ClientLost, "Imaging app stopped checking",
                                                       "No request to the " + device + " for " + client.silentFor + " - last checked " +
                                                           client.lastChecked + ".")));
                    else if (!client.silent && rules.onClientBack)
                        alerts.push_back(withVars(make(AlertType::ClientBack, "Imaging app is back", "The " + device + " is being checked again.")));
                }

                if (disconnectPending[i] && (!disconnectSent || now - disconnectSentAt >= cooldown))
                {
                    disconnectPending[i] = false;
                    disconnectSent = true;
                    disconnectSentAt = now;
                    alerts.push_back(withVars(make(AlertType::ClientDisconnected, "Imaging app disconnected", "The " + device + " was disconnected.")));
                }
            }
        }

    } // namespace Alerts
} // namespace SQM
