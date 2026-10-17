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
#include "AlertSchedule.h"
#include "ClientWatch.h"
#include "Config.h"
#include "DisplayFrame.h"
#include "ObservingConditionsMapper.h"
#include "Readings.h"
#include "SafetyEvaluator.h"
#include "SafetyStatus.h"
#include "SettingsDeps.h"
#include "SensorTypes.h"
#include "calculations/CloudDetection.h"
#include "calculations/Dewpoint.h"
#include "calculations/SkyQuality.h"

namespace SQM
{
    // One sky sensor (TSL2591) as read by its driver (spec 030). `enabled`
    // and `metrics` are filled by Core::derive.
    struct SkySensorSnapshot
    {
        bool enabled = false;
        bool initialized = false; // begun and answering
        TSL2591Reading reading;
        TSL2591Diagnostics diagnostics{};
        uint32_t lastUpdate = 0; // millis() of the last good read, 0 = never
        SkyQualityMetrics metrics;
    };

    // The fuel gauge and charger (spec 032). The driver fills `detected`,
    // `initialized`, `lastUpdate` and `reading` (with monitoring off it only
    // probes once at boot and reads nothing); the power manager `power` and
    // `shutdownInSeconds`; Core::derive the rest.
    struct BatterySnapshot
    {
        bool detected = false;    // a gauge answered at 0x36 (the boot probe counts)
        bool initialized = false; // monitoring on and the gauge answering
        uint32_t lastUpdate = 0;  // millis() of the last good gauge read, 0 = never
        BatteryReading reading;
        Battery::PowerState power = Battery::PowerState::Normal;
        uint32_t shutdownInSeconds = 0; // while power == ShutdownPending

        // Core::derive.
        bool enabled = false; // battery.enabled
        // Reported: monitoring on, and a gauge found or the charger wired (a
        // pack pulled out on USB still reports "no battery" from the charger).
        bool present = false;
        std::optional<uint32_t> timeToEmptyMinutes;
        std::optional<uint32_t> timeToFullMinutes;
        std::optional<float> currentMa;
    };

    // One consistent copy of every sensor's latest reading.
    struct SensorSnapshot
    {
        // Both sky sensors, indexed by SkyRole. The drivers fill these.
        SkySensorSnapshot skySensors[SKY_ROLE_COUNT];
        // The authoritative sky sensor (cfg.sky.authoritative); set by Core::derive.
        SkyRole skySource = SkyRole::Sqm;
        // The authoritative copy (Core::derive): every single-value consumer
        // (readings light/sky, safety, Alpaca, BLE, alerts, status) reads these.
        // Empty and not initialised when the authoritative sensor is off - never
        // the other sensor's values (FR-010).
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
        BatterySnapshot battery;
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
        // ArduinoJson pool for /api/status (src/WebServerStatus.cpp): the
        // sections this library writes plus the device's own (firmware,
        // memory, partitions, time, WiFi, MQTT, connections), worst case with
        // both sky sensors, pinned by test/test_device_core
        // (test_status_document_fits).
        constexpr size_t STATUS_DOC_CAPACITY = 7680;

        inline uint32_t ageMs(uint32_t now, uint32_t timestamp)
        {
            return timestamp == 0 ? 0 : now - timestamp;
        }

        // Fills the derived fields (each sky sensor's metrics, the
        // authoritative sky copy, cloud model) from the raw readings.
        void derive(SensorSnapshot &snapshot, const Config &cfg);

        // The readings document's content (lib/Readings writes it).
        Readings::Snapshot buildReadings(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs, int64_t epoch);
        // Samples the light sensor's averaging window holds when full.
        uint16_t windowSamples(const TSL2591Diagnostics &diag);
        // Bring-up diagnostics (/api/status "diagnostics", MQTT <base>/diagnostics).
        void writeDiagnostics(JsonObject root, const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs);
        // /api/status "skySensors": health and diagnostics of each enabled sky
        // sensor (spec 030); nothing when neither is enabled.
        void writeSkySensorStatus(JsonObject root, const SensorSnapshot &snapshot, const Readings::Snapshot &readings);
        // Dark calibration (POST /api/sensors/tsl2591/calibrate-dark): stores
        // the covered window's average as `role`'s dark offset and touches no
        // other setting (spec 030 US3). `at` is Unix seconds, or millis()
        // before the clock is set.
        void applyDarkCalibration(Config &cfg, SkyRole role, const TSL2591Diagnostics &diagnostics, int64_t at);
        // /api/status "battery" (the readings object, plus shutdownInSeconds
        // while a shutdown is pending) and "diagnostics.battery" (only with a
        // gauge), spec 032.
        void writeBatteryStatus(JsonObject root, const SensorSnapshot &snapshot, const Readings::Snapshot &readings);
        void writeBatteryDiagnostics(JsonObject root, const SensorSnapshot &snapshot, const Config &cfg);
        // Per-sensor health (/api/status "sensors").
        void writeSensorHealth(JsonObject sensors, const Readings::Snapshot &readings, const Config &cfg);

        // The sensor and GPS facts settings dependencies need (the caller
        // adds network, clock and Bluetooth facts).
        void sensorFacts(Deps::Facts &facts, const SensorSnapshot &snapshot, const Readings::Snapshot &readings);

        // Safety verdict inputs and limits.
        Alpaca::SafetyInputs safetyInputs(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs);
        Alpaca::SafetyThresholds safetyThresholds(const Config &cfg);
        // What Alpaca ObservingConditions reports.
        Alpaca::ObservingConditionsSnapshot observingConditions(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs);

        // Applies the safe delay to a fresh evaluation and updates `status`.
        // Returns true if the reported verdict changed (or this is the first).
        bool updateSafety(
            SafetyStatus &status, Alpaca::SafeDelayFilter &filter, const Alpaca::SafetyResult &result, const Config &cfg, uint32_t nowMs);
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

        // Alpaca's Location: the location in use (a GPS fix, else the saved
        // location) as "lat, lon", or "" when none is known.
        std::string alpacaLocation(const SensorSnapshot &snapshot, const Config &cfg);
        // /api/status "sky".
        void writeSky(JsonObject sky, const NightState &night);

        // Alerts.
        // The device state an alert pass reads: settings, readings, darkness
        // and the safety verdict.
        // Each sky sensor's own reading for the alert wording (spec 030):
        // {sqm_wide}, {sqm_l}, {sqm_delta}, {sqm_source}.
        struct SkyReadings
        {
            struct Role
            {
                bool valid = false; // on, ok and fresh
                float sqm = 0.0F;
            } roles[SKY_ROLE_COUNT];
            SkyRole source = SkyRole::Sqm;
        };
        SkyReadings skyReadings(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs);

        struct AlertSources
        {
            const Config &cfg;
            const Alpaca::ObservingConditionsSnapshot &obs;
            const NightState &night;
            const SafetyStatus &status;
            SkyReadings sky{}; // none given: every per-sensor value reads "--"
        };
        // The device's local time ("HH:MM") and date for {time} and {date}.
        struct LocalClock
        {
            std::string time;
            std::string date;
        };
        Alerts::AlertInputs alertInputs(const AlertSources &sources, const SensorSnapshot &snapshot, uint32_t nowMs);
        Alerts::AlertRules alertRules(const Config &cfg);
        const AlertsConfig::EventSetting *eventSettingFor(const AlertsConfig &alerts, Alerts::AlertType type);
        // Template variables for an alert: readings, settings, then the event's own.
        std::vector<std::pair<std::string, std::string>> alertVars(
            const AlertSources &sources, const Alerts::Alert &alert, const LocalClock &clock);
        void applyAlertTemplate(
            Alerts::Alert &alert, const AlertsConfig::EventSetting &setting, const std::vector<std::pair<std::string, std::string>> &vars);

        // The imaging app (specs/021): silence time per Alpaca device, the
        // client state as alert inputs, and the documents that report it.
        void clientSilenceMs(const Config &cfg, uint32_t (&out)[Alpaca::DEVICE_COUNT]);
        // `localTime` is "HH:MM" when the clock is set (else anything else).
        void addClientInputs(
            Alerts::AlertInputs &inputs, const Alpaca::ClientWatch &watch, const Config &cfg, uint32_t nowMs, const std::string &localTime);
        // /api/status "alpaca": {enabled, clients: {safetymonitor, observingconditions}}.
        void writeClientWatch(JsonObject alpaca, const Alpaca::ClientWatch &watch, const Config &cfg, uint32_t nowMs);
        // "2 min", "45 s", "1 h 5 min".
        std::string formatDuration(uint32_t seconds);

        // When alerts go out: the setting, and the /api/alerts/armed document
        // (also /api/status "alerts").
        Alerts::SendMode sendMode(const Config &cfg);
        // The mode the schedule acts on: the saved one, or "any time" while Alpaca
        // is off (specs/020-settings-dependencies D-12).
        Alerts::SendMode effectiveSendMode(const Config &cfg);
        void writeAlertSchedule(JsonObject target, const Alerts::ScheduleState &state, const Config &cfg, uint32_t nowMs);

        // One alert pass: the engine's raw alerts given their configured
        // level, sound and wording; Off events dropped.
        struct AlertStep
        {
            std::vector<Alerts::Alert> outgoing;
            uint32_t alarmFlags = 0; // Bluetooth alarm reasons for Wake-level alerts
            // The battery rule's "ring paired phones" (spec 032): ring with
            // alarmFlags whatever the notification's level, alert sent or not.
            bool ringPhones = false;
        };
        AlertStep runAlerts(
            Alerts::AlertEngine &engine,
            const Alerts::AlertInputs &inputs,
            const Alerts::AlertRules &rules,
            const AlertSources &sources,
            const LocalClock &clock);

        // "Test" on an event row of the Alerts settings: a sample of each event.
        struct SampleAlert
        {
            const char *key; // event key, e.g. "rain_started"
            Alerts::AlertType type;
            const char *title; // built-in title
            const char *label; // "Rain starts"
            uint32_t bleFlags; // Bluetooth alarm reasons at Wake level
        };
        const SampleAlert *sampleAlert(const std::string &key); // nullptr if unknown
        // The test notification: the generic one (sample == nullptr) or a
        // sample of an event in the given level, sound and wording, filled in
        // from live readings.
        // The level, sound and wording chosen for a test of an event.
        struct TestWording
        {
            uint8_t level = 0;
            std::string sound;
            std::string title;
            std::string message;
        };
        Alerts::Alert buildTestAlert(
            const SampleAlert *sample, const TestWording &wording, const AlertSources &sources, const LocalClock &clock);

        // ISO 8601 UTC, or "" before the clock is set.
        std::string isoUtc(int64_t epoch);

        // The OLED display (specs/031-oled-display, src/DeviceDisplay.cpp).
        // The network as WiFiManager sees it.
        struct DisplayNetwork
        {
            bool connected = false; // joined a network
            bool apMode = false;    // the setup hotspot is up
            std::string ip;         // the address in use
            std::string apName;     // the hotspot's name
        };
        // What each page may show, from the readings snapshot every interface
        // uses (research D4): a stale or faulted group is invalid, never a value.
        Display::View displayView(
            const SensorSnapshot &snapshot,
            const Readings::Snapshot &readings,
            const SafetyStatus &safety,
            const Config &cfg,
            const DisplayNetwork &network);
        // The controller's state for /api/status.
        struct DisplayStatus
        {
            bool detected = false;
            bool on = false;
            Display::Page page = Display::Page::Sky; // when on
            bool nightMode = false;
        };
        // /api/status "display" (contracts/status-and-routes.md), always present.
        void writeDisplayStatus(JsonObject root, const Config &cfg, const DisplayStatus &state);
        // Deps::Facts.displayDetected: a display answered and it is on in settings.
        void displayFacts(Deps::Facts &facts, const Config &cfg, bool detected);
    } // namespace Core
} // namespace SQM
