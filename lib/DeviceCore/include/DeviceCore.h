#pragma once

// What the device decides from its sensors and settings, independent of the
// hardware: readings, the safety verdict's inputs, darkness, alert decisions
// and wording, and the documents the web UI and Alpaca read. The firmware
// (src/WebServer.cpp), the native tests and the browser demo all run this.

#include <ArduinoJson.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "AlertEngine.h"
#include "Config.h"
#include "ObservingConditionsMapper.h"
#include "Readings.h"
#include "SafetyEvaluator.h"
#include "SafetyStatus.h"
#include "SensorTypes.h"
#include "calculations/CloudDetection.h"
#include "calculations/Dewpoint.h"
#include "calculations/SkyQuality.h"

namespace SQM
{
    // One consistent copy of every sensor's latest reading.
    struct SensorSnapshot
    {
        TSL2591Reading tsl;
        TSL2591Diagnostics tslDiagnostics;
        BME280Reading bme;
        MLX90614Reading mlx;
        GPSReading gps;
        RG15Reading rg15;
        RG15Diagnostics rg15Diagnostics;
        bool gpsInitialized = false;
        bool rg15Initialized = false;
        bool tslInitialized = false;
        bool bmeInitialized = false;
        bool mlxInitialized = false;
        uint32_t tslLastUpdate = 0;
        uint32_t bmeLastUpdate = 0;
        uint32_t mlxLastUpdate = 0;
        uint32_t gpsLastUpdate = 0;
        uint32_t rg15LastUpdate = 0;
        WindReading wind;
        uint32_t dataTimestamp = 0;
        uint32_t capturedAt = 0;

        // Derived once per reading (Core::derive) so REST, MQTT, Alpaca and
        // the safety verdict all see the same numbers.
        SkyQualityMetrics sky;
        CloudMetrics cloud;
        bool humidityMeasured = false;                  // else the cloud model assumed:
        float cloudHumidity = ASSUMED_HUMIDITY_PERCENT; // humidity it used
    };

    namespace Core
    {
        // A clock reading before this (2024-01-01) means "not set yet".
        constexpr int64_t CLOCK_VALID_EPOCH = 1704067200;
        // A reading is stale this long after the read interval.
        constexpr uint32_t SENSOR_STALE_GRACE_MS = 1000;

        inline uint32_t ageMs(uint32_t now, uint32_t timestamp) { return timestamp == 0 ? 0 : now - timestamp; }

        // Fills the derived fields (sky quality, cloud model) from the raw readings.
        void derive(SensorSnapshot &snapshot, const Config &cfg);

        // The readings document's content (lib/Readings writes it).
        Readings::Snapshot buildReadings(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs, int64_t epoch);
        // Samples the light sensor's averaging window holds when full.
        uint16_t windowSamples(const TSL2591Diagnostics &diag);
        // Bring-up diagnostics (/api/status "diagnostics", MQTT <base>/diagnostics).
        void writeDiagnostics(JsonObject root, const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs);
        // Per-sensor health (/api/status "sensors").
        void writeSensorHealth(JsonObject sensors, const Readings::Snapshot &readings, const Config &cfg);

        // Safety verdict inputs and limits.
        Alpaca::SafetyInputs safetyInputs(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs);
        Alpaca::SafetyThresholds safetyThresholds(const Config &cfg);
        // What Alpaca ObservingConditions reports.
        Alpaca::ObservingConditionsSnapshot observingConditions(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs);

        // Applies the safe delay to a fresh evaluation and updates `status`.
        // Returns true if the reported verdict changed (or this is the first).
        bool updateSafety(SafetyStatus &status, Alpaca::SafeDelayFilter &filter, const Alpaca::SafetyResult &result,
                          const Config &cfg, uint32_t nowMs);
        // The safety document (/api/safety, readings "safety", MQTT <base>/safety).
        void writeSafety(JsonObject target, const SafetyStatus &status, const Config &cfg, uint32_t nowMs);

        // Darkness at the device: GPS fix, else the location in settings.
        struct NightState
        {
            const char *source = nullptr; // "gps", "manual" or null
            double latitude = 0.0;
            double longitude = 0.0;
            bool known = false;
            bool isNight = false;
            double sunAltitudeDeg = 0.0;
        };
        NightState night(const SensorSnapshot &snapshot, const Config &cfg, int64_t epoch);
        // /api/status "sky".
        void writeSky(JsonObject sky, const NightState &night);

        // Alerts.
        Alerts::AlertInputs alertInputs(const SafetyStatus &status, const SensorSnapshot &snapshot,
                                        const Alpaca::ObservingConditionsSnapshot &obs, const Config &cfg,
                                        const NightState &night, uint32_t nowMs);
        Alerts::AlertRules alertRules(const Config &cfg);
        const AlertsConfig::EventSetting *eventSettingFor(const AlertsConfig &alerts, Alerts::AlertType type);
        // Template variables for an alert: readings, settings, then the event's own.
        std::vector<std::pair<std::string, std::string>> alertVars(const Config &cfg, const Alpaca::ObservingConditionsSnapshot &obs,
                                                                   const NightState &night, const Alerts::Alert &alert,
                                                                   const std::string &localTime, const std::string &localDate);
        void applyAlertTemplate(Alerts::Alert &alert, const AlertsConfig::EventSetting &setting,
                                const std::vector<std::pair<std::string, std::string>> &vars);

        // One alert pass: the engine's raw alerts given their configured
        // level, sound and wording; Off events dropped.
        struct AlertStep
        {
            std::vector<Alerts::Alert> outgoing;
            uint32_t alarmFlags = 0; // Bluetooth alarm reasons for Wake-level alerts
        };
        AlertStep runAlerts(Alerts::AlertEngine &engine, const Alerts::AlertInputs &inputs, const Alerts::AlertRules &rules,
                            const Config &cfg, const Alpaca::ObservingConditionsSnapshot &obs, const NightState &night,
                            const SafetyStatus &status, const std::string &localTime, const std::string &localDate);

        // ISO 8601 UTC, or "" before the clock is set.
        std::string isoUtc(int64_t epoch);
    } // namespace Core
} // namespace SQM
