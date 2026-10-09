#include "DeviceCore.h"

#include <cstdio>

// The alert side of the device core: alert inputs from the device state,
// the rules and wording from settings, the imaging-app (Alpaca client)
// watch, the send schedule documents and test notifications. Readings and
// the safety verdict are in DeviceCore.cpp.

namespace SQM
{
    namespace Core
    {
        Alerts::AlertInputs alertInputs(const AlertSources &sources, const SensorSnapshot &snapshot, uint32_t now)
        {
            const SafetyStatus &status = sources.status;
            const Alpaca::ObservingConditionsSnapshot &obs = sources.obs;
            const Config &cfg = sources.cfg;
            const NightState &n = sources.night;
            Alerts::AlertInputs in;
            in.nowSeconds = now / 1000;
            in.safetyKnown = status.evaluatedAtMs != 0;
            in.safetySettling = (!status.isSafe && status.rawSafe) || (status.reasonFlags & Alpaca::UnsafeNoData) != 0;
            in.isSafe = status.isSafe;
            in.unsafeReasons = status.reasons;

            in.rainEnabled = cfg.rain.enabled;
            in.raining = snapshot.rg15.isRaining || snapshot.rg15.rainLatched;
            in.rainRateMmPerHour = obs.rainRateMmPerHour;
            in.lensFault = snapshot.rg15.lensBad;

            in.sensors[0] = {"Light sensor", true, obs.skyLight.valid};
            in.sensors[1] = {"IR sky sensor", true, obs.irSky.valid};
            in.sensors[2] = {"Environment sensor", true, obs.environment.valid};
            in.sensors[3] = {"Rain sensor", cfg.rain.enabled, obs.rain.valid};
            in.sensors[4] = {"Anemometer", obs.wind.present, obs.wind.valid};

            in.environmentValid = obs.environment.valid;
            in.temperatureC = obs.temperatureC;
            in.dewpointC = obs.dewpointC;
            in.skyValid = obs.irSky.valid;
            in.cloudCoverPercent = obs.cloudCoverPercent;
            in.nightKnown = n.known;
            in.isNight = n.isNight;
            return in;
        }

        Alerts::AlertRules alertRules(const Config &cfg)
        {
            Alerts::AlertRules rules;
            const AlertsConfig &a = cfg.alerts;
            rules.onSafetyChange = a.unsafe.level || a.safe.level;
            rules.onRain = a.rainStarted.level || a.rainStopped.level;
            rules.onSensorFault = a.sensorFault.level || a.sensorRecovered.level;
            rules.onDewRisk = a.dewRisk.level != 0;
            rules.dewRiskMarginC = a.dewRiskMarginC;
            rules.onClearSky = a.clearSky.level != 0;
            rules.clearSkyCloudPercent = a.clearSkyCloudPercent;
            rules.onCloudedOver = a.cloudedOver.level != 0;
            rules.cloudedOverCloudPercent = a.cloudedOverCloudPercent;
            rules.skyNightOnly = a.skyNightOnly;
            rules.safetyNightOnly = a.safetyNightOnly;
            rules.cooldownSeconds = a.cooldownSeconds;
            rules.onClientLost = a.clientLost.level != 0;
            rules.onClientBack = a.clientBack.level != 0;
            rules.onClientDisconnected = a.clientDisconnected.level != 0;
            return rules;
        }

        const AlertsConfig::EventSetting *eventSettingFor(const AlertsConfig &a, Alerts::AlertType type)
        {
            switch (type)
            {
            case Alerts::AlertType::Unsafe:
                return &a.unsafe;
            case Alerts::AlertType::Safe:
                return &a.safe;
            case Alerts::AlertType::RainStarted:
                return &a.rainStarted;
            case Alerts::AlertType::RainStopped:
                return &a.rainStopped;
            case Alerts::AlertType::SensorFault:
            case Alerts::AlertType::LensFault:
                return &a.sensorFault;
            case Alerts::AlertType::SensorRecovered:
                return &a.sensorRecovered;
            case Alerts::AlertType::DewRisk:
                return &a.dewRisk;
            case Alerts::AlertType::ClearSky:
                return &a.clearSky;
            case Alerts::AlertType::CloudedOver:
                return &a.cloudedOver;
            case Alerts::AlertType::ClientLost:
                return &a.clientLost;
            case Alerts::AlertType::ClientBack:
                return &a.clientBack;
            case Alerts::AlertType::ClientDisconnected:
                return &a.clientDisconnected;
            default:
                return nullptr;
            }
        }

        std::vector<std::pair<std::string, std::string>> alertVars(
            const AlertSources &sources, const Alerts::Alert &alert, const LocalClock &clock)
        {
            const Config &cfg = sources.cfg;
            const Alpaca::ObservingConditionsSnapshot &obs = sources.obs;
            const NightState &n = sources.night;
            auto num = [](bool valid, double value, int decimals) -> std::string
            {
                if (!valid)
                    return "--";
                char buffer[24];
                std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
                return buffer;
            };
            const AlertsConfig &a = cfg.alerts;
            const AlpacaConfig &limits = cfg.alpaca;

            // Readings and settings first; the event's own values (reasons,
            // sensor, ...) come after and win on a name clash.
            std::vector<std::pair<std::string, std::string>> vars = {
                {"device", cfg.deviceName},
                {"event", Alerts::alertTypeName(alert.type)},
                {"level", Alerts::alertLevelName(alert.level)},
                {"time", clock.time},
                {"date", clock.date},
                {"sqm", num(obs.skyLight.valid, obs.skyQualityMagArcsec2, 2)},
                {"sqm_min", num(true, limits.sqmMinSafe, 2)},
                {"cloud", num(obs.irSky.valid, obs.cloudCoverPercent, 0)},
                {"cloud_max", num(true, limits.cloudCoverUnsafePercent, 0)},
                {"clear_below", num(true, a.clearSkyCloudPercent, 0)},
                {"cloudy_above", num(true, a.cloudedOverCloudPercent, 0)},
                {"sky_temp", num(obs.irSky.valid, obs.skyTemperatureC, 1)},
                {"temp", num(obs.environment.valid, obs.temperatureC, 1)},
                {"humidity", num(obs.environment.valid, obs.humidityPercent, 0)},
                {"humidity_max", num(true, limits.humidityMaxSafe, 0)},
                {"dewpoint", num(obs.environment.valid, obs.dewpointC, 1)},
                {"dew_margin", num(obs.environment.valid, obs.temperatureC - obs.dewpointC, 1)},
                {"pressure", num(obs.environment.valid, obs.pressureHPa, 0)},
                {"rain_rate", num(obs.rain.valid, obs.rainRateMmPerHour, 1)},
                {"wind", num(obs.wind.valid, obs.windSpeedMs, 1)},
                {"gust", num(obs.wind.valid, obs.windGustMs, 1)},
                {"sun_alt", num(n.known, n.sunAltitudeDeg, 1)},
            };
            vars.insert(vars.end(), alert.vars.begin(), alert.vars.end());
            return vars;
        }

        void applyAlertTemplate(
            Alerts::Alert &alert, const AlertsConfig::EventSetting &setting, const std::vector<std::pair<std::string, std::string>> &vars)
        {
            if (!setting.title.empty())
                alert.title = Alerts::renderTemplate(setting.title, vars);
            if (!setting.message.empty())
                alert.message = Alerts::renderTemplate(setting.message, vars);
            // Not needed past this point, and the recent-alerts list keeps alerts.
            alert.vars.clear();
            alert.vars.shrink_to_fit();
        }

        void clientSilenceMs(const Config &cfg, uint32_t (&out)[Alpaca::DEVICE_COUNT])
        {
            out[static_cast<size_t>(Alpaca::Device::SafetyMonitor)] = cfg.alerts.clientSilentSafetySeconds * 1000;
            out[static_cast<size_t>(Alpaca::Device::ObservingConditions)] = cfg.alerts.clientSilentWeatherSeconds * 1000;
        }

        std::string formatDuration(uint32_t seconds)
        {
            if (seconds < 60)
                return std::to_string(seconds) + " s";
            const uint32_t minutes = (seconds + 30) / 60;
            if (minutes < 60)
                return std::to_string(minutes) + " min";
            return std::to_string(minutes / 60) + " h" + (minutes % 60 != 0 ? " " + std::to_string(minutes % 60) + " min" : "");
        }

        namespace
        {
            const char *const CLIENT_DEVICE_NAMES[Alpaca::DEVICE_COUNT] = {"safety monitor", "weather device"};
            const char *const CLIENT_DEVICE_KEYS[Alpaca::DEVICE_COUNT] = {"safetymonitor", "observingconditions"};

            // The local clock time `ageSeconds` ago, from the current "HH:MM";
            // "N min ago" when there's no clock.
            std::string lastCheckedText(uint32_t ageSeconds, const std::string &localTime)
            {
                const bool clock = localTime.size() == 5 && localTime[2] == ':' && std::isdigit(static_cast<unsigned char>(localTime[0])) &&
                                   std::isdigit(static_cast<unsigned char>(localTime[1])) &&
                                   std::isdigit(static_cast<unsigned char>(localTime[3])) &&
                                   std::isdigit(static_cast<unsigned char>(localTime[4]));
                if (!clock)
                    return formatDuration(ageSeconds) + " ago";
                const int now = std::stoi(localTime.substr(0, 2)) * 60 + std::stoi(localTime.substr(3, 2));
                const int then = ((now - static_cast<int>(ageSeconds / 60)) % 1440 + 1440) % 1440;
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "%02d:%02d", then / 60, then % 60);
                return buffer;
            }
        } // namespace

        void addClientInputs(
            Alerts::AlertInputs &inputs, const Alpaca::ClientWatch &watch, const Config &cfg, uint32_t nowMs, const std::string &localTime)
        {
            uint32_t silence[Alpaca::DEVICE_COUNT];
            clientSilenceMs(cfg, silence);
            for (size_t i = 0; i < Alpaca::DEVICE_COUNT; ++i)
            {
                const Alpaca::ClientState &state = watch.state(static_cast<Alpaca::Device>(i));
                Alerts::ClientInputs &client = inputs.clients[i];
                client.device = CLIENT_DEVICE_NAMES[i];
                client.watching = state.watching;
                client.silent = state.silent;
                client.disconnectedNow = state.disconnectedNow;
                client.silentFor = formatDuration(silence[i] / 1000);
                client.lastChecked = state.everRequested ? lastCheckedText((nowMs - state.lastRequestMs) / 1000, localTime) : "never";
                client.clientId = state.hasClientId ? std::to_string(state.clientId) : "";
            }
        }

        void writeClientWatch(JsonObject alpaca, const Alpaca::ClientWatch &watch, const Config &cfg, uint32_t nowMs)
        {
            alpaca["enabled"] = cfg.alpaca.enabled;
            JsonObject clients = alpaca.createNestedObject("clients");
            for (size_t i = 0; i < Alpaca::DEVICE_COUNT; ++i)
            {
                const Alpaca::ClientState &state = watch.state(static_cast<Alpaca::Device>(i));
                JsonObject client = clients.createNestedObject(CLIENT_DEVICE_KEYS[i]);
                client["connected"] = state.connected;
                client["watching"] = state.watching;
                client["silent"] = state.silent;
                if (state.everRequested)
                    client["lastCheckedAgeMs"] = nowMs - state.lastRequestMs;
                else
                    client["lastCheckedAgeMs"] = nullptr;
                if (state.hasClientId)
                    client["clientId"] = state.clientId;
                else
                    client["clientId"] = nullptr;
            }
        }

        Alerts::SendMode sendMode(const Config &cfg)
        {
            return cfg.alerts.sendMode == AlertsConfig::SendMode::WhileConnected ? Alerts::SendMode::WhileConnected : Alerts::SendMode::Any;
        }

        Alerts::SendMode effectiveSendMode(const Config &cfg)
        {
            // dep: D-12 - imaging apps connect over Alpaca: with it off, "only while
            // an imaging app is connected" isn't in effect and alerts go out any time.
            return cfg.alpaca.enabled ? sendMode(cfg) : Alerts::SendMode::Any;
        }

        void writeAlertSchedule(JsonObject target, const Alerts::ScheduleState &state, const Config &cfg, uint32_t nowMs)
        {
            target["armed"] = state.sending;
            target["armWithAlpaca"] = cfg.alerts.sendMode == AlertsConfig::SendMode::WhileConnected;
            target["mode"] = Alerts::sendModeName(sendMode(cfg));
            target["reason"] = Alerts::scheduleReasonName(state.reason);
            const std::string since = isoUtc(state.sinceEpoch);
            if (since.empty())
                target["since"] = nullptr;
            else
                target["since"] = since;
            if (state.sinceKnown)
                target["sinceAgeMs"] = nowMs - state.sinceMs;
            else
                target["sinceAgeMs"] = nullptr;
        }

        AlertStep runAlerts(
            Alerts::AlertEngine &engine,
            const Alerts::AlertInputs &inputs,
            const Alerts::AlertRules &rules,
            const AlertSources &sources,
            const LocalClock &clock)
        {
            const Config &cfg = sources.cfg;
            AlertStep step;
            for (Alerts::Alert alert : engine.update(inputs, rules))
            {
                const AlertsConfig::EventSetting *setting = eventSettingFor(cfg.alerts, alert.type);
                if (setting == nullptr || setting->level == 0)
                    continue;
                alert.level = static_cast<Alerts::AlertLevel>(setting->level);
                alert.sound = setting->sound;
                applyAlertTemplate(alert, *setting, alertVars(sources, alert, clock));
                if (alert.level == Alerts::AlertLevel::Wake)
                    step.alarmFlags |= sources.status.reasonFlags |
                                       (alert.type == Alerts::AlertType::RainStarted ? Alpaca::UnsafeRain : 0u) |
                                       (alert.type == Alerts::AlertType::SensorFault || alert.type == Alerts::AlertType::LensFault
                                            ? Alpaca::UnsafeSensorFault
                                            : 0u);
                step.outgoing.push_back(std::move(alert));
            }
            return step;
        }

        namespace
        {
            constexpr SampleAlert SAMPLE_ALERTS[] = {
                {"unsafe", Alerts::AlertType::Unsafe, "Observatory UNSAFE", "It turns unsafe", Alpaca::UnsafeCloudCover},
                {"safe", Alerts::AlertType::Safe, "Observatory safe", "It's safe again", 0},
                {"rain_started", Alerts::AlertType::RainStarted, "Rain detected", "Rain starts", Alpaca::UnsafeRain},
                {"rain_stopped", Alerts::AlertType::RainStopped, "Rain cleared", "Rain stops", 0},
                {"sensor_fault", Alerts::AlertType::SensorFault, "Sensor fault", "A sensor fails", Alpaca::UnsafeSensorFault},
                {"sensor_recovered", Alerts::AlertType::SensorRecovered, "Sensor recovered", "A sensor recovers", 0},
                {"dew_risk", Alerts::AlertType::DewRisk, "Dew risk", "Dew risk", Alpaca::UnsafeDewpoint},
                {"clear_sky", Alerts::AlertType::ClearSky, "Dark and clear", "Skies clear up", 0},
                {"clouded_over", Alerts::AlertType::CloudedOver, "Clouded over", "Skies cloud over", Alpaca::UnsafeCloudCover},
                {"client_lost", Alerts::AlertType::ClientLost, "Imaging app stopped checking", "The imaging app stops checking", 0},
                {"client_back", Alerts::AlertType::ClientBack, "Imaging app is back", "The imaging app is back", 0},
                {"client_disconnected",
                 Alerts::AlertType::ClientDisconnected,
                 "Imaging app disconnected",
                 "The imaging app disconnects",
                 0},
            };
        } // namespace

        const SampleAlert *sampleAlert(const std::string &key)
        {
            for (const SampleAlert &sample : SAMPLE_ALERTS)
                if (key == sample.key)
                    return &sample;
            return nullptr;
        }

        Alerts::Alert buildTestAlert(
            const SampleAlert *sample, const TestWording &wording, const AlertSources &sources, const LocalClock &clock)
        {
            const SafetyStatus &safety = sources.status;
            const Config &cfg = sources.cfg;
            const uint8_t level = wording.level;
            Alerts::Alert test;
            if (sample == nullptr)
            {
                test.type = Alerts::AlertType::Test;
                test.level = Alerts::AlertLevel::Normal;
                test.title = "Test notification";
                test.message = "Alerts from this SQMeter are working.";
                return test;
            }
            test.type = sample->type;
            test.level = static_cast<Alerts::AlertLevel>(level);
            test.sound = wording.sound;
            test.title = sample->title;
            test.message = std::string("This is how a \"") + sample->label + "\" alert arrives.";

            // Custom wording is filled in from live readings; values only a
            // real event has (the reasons, which sensor) are examples unless
            // they apply right now.
            if (sample->type == Alerts::AlertType::Unsafe)
            {
                const std::vector<std::string> reasons =
                    safety.isSafe ? std::vector<std::string>{"Cloud 62% >= 35% (example)"} : safety.reasons;
                std::string reasonsInline;
                for (const std::string &reason : reasons)
                    reasonsInline += (reasonsInline.empty() ? "" : "; ") + reason;
                test.vars = {
                    {"reasons", Alerts::joinReasons(reasons)},
                    {"reasons_inline", reasonsInline},
                    {"reason_count", std::to_string(reasons.size())}};
            }
            else if (sample->type == Alerts::AlertType::SensorFault || sample->type == Alerts::AlertType::SensorRecovered)
                test.vars = {{"sensor", "Light sensor (example)"}};
            else if (
                sample->type == Alerts::AlertType::ClientLost || sample->type == Alerts::AlertType::ClientBack ||
                sample->type == Alerts::AlertType::ClientDisconnected)
                test.vars = {
                    {"device", "safety monitor"},
                    {"silent_for", formatDuration(cfg.alerts.clientSilentSafetySeconds)},
                    {"last_checked", "2 min ago (example)"},
                    {"client_id", "1234 (example)"}};
            AlertsConfig::EventSetting custom{level, wording.sound, wording.title, wording.message};
            applyAlertTemplate(test, custom, alertVars(sources, test, clock));
            test.title = "Test: " + test.title;
            return test;
        }
    } // namespace Core
} // namespace SQM
