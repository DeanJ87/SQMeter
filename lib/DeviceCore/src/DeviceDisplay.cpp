#include "DeviceCore.h"

#include <cstdio>

// The OLED's view model and status (specs/031-oled-display, research D4/D9).

namespace SQM
{
    namespace Core
    {
        namespace
        {
            template <size_t N> void copyText(char (&target)[N], const std::string &text)
            {
                std::snprintf(target, N, "%s", text.c_str());
            }

            void skyGroup(Display::View::Sky &sky, const Readings::Snapshot &readings, const Config &cfg)
            {
                const SkyRole source = readings.source;
                const SkyRole other = source == SkyRole::Sqm ? SkyRole::Sqml : SkyRole::Sqm;
                sky.available = cfg.skyEnabled(cfg.sky.authoritative);
                sky.label = skyRoleLabel(source);
                sky.valid = sky.available && readings.light.status == Readings::Status::Ok;
                sky.sqm = static_cast<float>(readings.sky.sqm);
                sky.bortle = readings.sky.bortle;
                sky.nelm = static_cast<float>(readings.sky.nelm);
                const Readings::SkySensorEntry &entry = readings.skySensors[static_cast<size_t>(other)];
                sky.other.present = entry.present && cfg.skyEnabled(other);
                sky.other.valid = sky.other.present && entry.status == Readings::Status::Ok;
                sky.other.sqm = static_cast<float>(entry.sqm);
                sky.other.label = skyRoleLabel(other);
            }
        } // namespace

        Display::View displayView(
            const SensorSnapshot &snapshot,
            const Readings::Snapshot &readings,
            const SafetyStatus &safety,
            const Config &cfg,
            const DisplayNetwork &network)
        {
            Display::View v;
            skyGroup(v.sky, readings, cfg);

            v.environment.available = snapshot.bmeInitialized;
            v.environment.valid = readings.environment.status == Readings::Status::Ok;
            v.environment.temperatureC = static_cast<float>(readings.environment.temperature);
            v.environment.humidity = static_cast<float>(readings.environment.humidity);
            v.environment.dewpointC = static_cast<float>(readings.environment.dewpoint);

            v.cloud.available = snapshot.mlxInitialized;
            v.cloud.valid = readings.infrared.status == Readings::Status::Ok;
            v.cloud.skyTemperatureC = static_cast<float>(readings.infrared.skyTemperature);
            v.cloud.condition = readings.clouds.condition;

            v.safety.known = safety.evaluatedAtMs != 0;
            v.safety.safe = safety.isSafe;
            if (v.safety.known && !safety.isSafe && !safety.reasons.empty())
                copyText(v.safety.firstReason, safety.reasons.front());

            v.gps.available = cfg.gps.enabled;
            v.gps.valid = readings.gps.status == Readings::Status::Ok;
            v.gps.fix = readings.gps.fix;
            v.gps.satellites = readings.gps.satellites;
            v.gps.latitude = readings.gps.latitude;
            v.gps.longitude = readings.gps.longitude;

            // v.battery stays unavailable until spec 032 adds the fuel gauge.

            copyText(v.network.name, cfg.deviceName);
            v.network.mode = network.apMode      ? Display::View::NetMode::Ap
                             : network.connected ? Display::View::NetMode::Wifi
                                                 : Display::View::NetMode::None;
            if (v.network.mode != Display::View::NetMode::None)
                copyText(v.network.ip, network.ip);
            copyText(v.network.apName, network.apName);
            return v;
        }

        void writeDisplayStatus(JsonObject root, const Config &cfg, const DisplayStatus &state)
        {
            JsonObject display = root.createNestedObject("display");
            const bool enabled = cfg.display.enabled;
            const bool detected = enabled && state.detected;
            const bool on = detected && state.on;
            display["enabled"] = enabled;
            display["detected"] = detected;
            display["on"] = on;
            if (on)
                display["page"] = Display::pageName(state.page);
            else
                display["page"] = nullptr;
            display["nightMode"] = state.nightMode;
        }

        void displayFacts(Deps::Facts &facts, const Config &cfg, bool detected)
        {
            facts.displayDetected = cfg.display.enabled && detected;
        }
    } // namespace Core
} // namespace SQM
