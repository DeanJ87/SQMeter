#pragma once

// Helpers shared by the ConfigModel sources: defaults (ConfigModel.cpp),
// JSON (ConfigJsonOut.cpp, ConfigJsonIn.cpp) and validation (ConfigValidate.cpp).

#include "Config.h"
#include <ArduinoJson.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace SQM::ConfigDetail
{
    inline constexpr const char *SECRET_MASK = "********";

    constexpr size_t EVENT_COUNT = 12;
    // Events from here on are the imaging-app events, persisted separately.
    constexpr size_t FIRST_CLIENT_EVENT = 9;

    inline bool setError(std::string *error, const std::string &message)
    {
        if (error)
            *error = message;
        return false;
    }

    inline bool inRange(float value, float min, float max)
    {
        return std::isfinite(value) && value >= min && value <= max;
    }

    // ESP32 GPIOs usable for the sensor buses (no flash pins, no 20/24/28-31).
    inline bool isValidGpio(int pin)
    {
        constexpr uint64_t VALID_PINS = (1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) | (1ULL << 4) | (1ULL << 5) | (1ULL << 12) |
                                        (1ULL << 13) | (1ULL << 14) | (1ULL << 15) | (1ULL << 16) | (1ULL << 17) | (1ULL << 18) |
                                        (1ULL << 19) | (1ULL << 21) | (1ULL << 22) | (1ULL << 23) | (1ULL << 25) | (1ULL << 26) |
                                        (1ULL << 27) | (1ULL << 32) | (1ULL << 33) | (1ULL << 34) | (1ULL << 35) | (1ULL << 36) |
                                        (1ULL << 39);
        return pin >= 0 && pin < 64 && ((VALID_PINS >> pin) & 1ULL) != 0;
    }

    inline bool isValidBaudRate(uint32_t baudRate)
    {
        constexpr uint32_t RATES[] = {2400, 4800, 9600, 19200, 38400, 57600, 115200};
        for (uint32_t rate : RATES)
            if (baudRate == rate)
                return true;
        return false;
    }

    inline bool isTimeSourceEnabled(const Config &cfg, TimeSource source)
    {
        return source == TimeSource::NTP ? cfg.ntp.enabled : cfg.gps.enabled;
    }

    // JSON key for each configurable event, matching the alert event names.
    template <typename Alerts, typename Setting> std::array<std::pair<const char *, Setting *>, EVENT_COUNT> eventSettingsOf(Alerts &a)
    {
        return {
            {{"unsafe", &a.unsafe},
             {"safe", &a.safe},
             {"rain_started", &a.rainStarted},
             {"rain_stopped", &a.rainStopped},
             {"sensor_fault", &a.sensorFault},
             {"sensor_recovered", &a.sensorRecovered},
             {"dew_risk", &a.dewRisk},
             {"clear_sky", &a.clearSky},
             {"clouded_over", &a.cloudedOver},
             {"client_lost", &a.clientLost},
             {"client_back", &a.clientBack},
             {"client_disconnected", &a.clientDisconnected}}};
    }
    inline std::array<std::pair<const char *, const AlertsConfig::EventSetting *>, EVENT_COUNT> eventSettings(const AlertsConfig &a)
    {
        return eventSettingsOf<const AlertsConfig, const AlertsConfig::EventSetting>(a);
    }
    inline std::array<std::pair<const char *, AlertsConfig::EventSetting *>, EVENT_COUNT> eventSettings(AlertsConfig &a)
    {
        return eventSettingsOf<AlertsConfig, AlertsConfig::EventSetting>(a);
    }

    inline const char *sendModeName(AlertsConfig::SendMode mode)
    {
        return mode == AlertsConfig::SendMode::WhileConnected ? "whileConnected" : "any";
    }

    // MQTT broker and webhook URL forms, IPv6 literals included (spec 015);
    // web/src/validation/configSchema.ts applies the same rules.
    bool validateAddresses(const Config &cfg, std::string *error);

    // Sets `target` from obj[key] when the key is present. `fallback` is used
    // for a value of the wrong type, exactly as `obj[key] | fallback` does.
    template <typename T, typename D> void take(JsonObject obj, const char *key, T &target, D fallback)
    {
        if (obj.containsKey(key))
            target = obj[key] | fallback;
    }
} // namespace SQM::ConfigDetail
