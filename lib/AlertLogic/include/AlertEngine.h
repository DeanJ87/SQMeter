#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SQM
{
    namespace Alerts
    {

        enum class AlertType : uint8_t
        {
            Unsafe,
            Safe,
            RainStarted,
            RainStopped,
            SensorFault,
            SensorRecovered,
            LensFault,
            DewRisk,
            ClearSky,
            CloudedOver,
            Acknowledged, // someone acknowledged a phone alarm
            Test,
        };

        // Maps onto Pushover / ntfy priorities by the dispatcher.
        enum class AlertPriority : int8_t
        {
            Low = -1,
            Normal = 0,
            High = 1, // urgent: rain, unsafe, sensor fault
        };

        struct Alert
        {
            AlertType type = AlertType::Test;
            AlertPriority priority = AlertPriority::Normal;
            std::string title;
            std::string message;
        };

        const char *alertTypeName(AlertType type);

        struct AlertRules
        {
            bool onSafetyChange = true;
            bool onRain = true;
            bool onSensorFault = true;
            bool onDewRisk = false;
            float dewRiskMarginC = 2.0f;
            // Sky changes. Clear below clearSkyCloudPercent, clouded over
            // above cloudedOverCloudPercent; in between nothing changes, so a
            // sky hovering near one threshold doesn't flip-flop.
            bool onClearSky = false;
            float clearSkyCloudPercent = 20.0f;
            bool onCloudedOver = false;
            float cloudedOverCloudPercent = 70.0f;
            uint32_t skySettleSeconds = 120;
            // Only announce sky changes while it's dark. Becoming dark while
            // the sky is already clear counts as "clear" (once).
            bool skyNightOnly = true;
            uint32_t cooldownSeconds = 300;
            // A sensor fault (or recovery) must hold this long before it's
            // announced, so blips - saving settings, reconfiguring a sensor,
            // an OTA upload stalling the loop - don't page anyone.
            uint32_t sensorSettleSeconds = 30;
            // Nothing is sent this soon after boot; state is still tracked,
            // so a reboot doesn't announce whatever the startup state is.
            uint32_t startupGraceSeconds = 60;
        };

        constexpr size_t SENSOR_COUNT = 5;

        struct SensorHealth
        {
            const char *name = "";
            bool enabled = false; // fitted / switched on - disabled sensors never alert
            bool healthy = false;
        };

        struct AlertInputs
        {
            uint32_t nowSeconds = 0; // monotonic

            bool safetyKnown = false;
            bool isSafe = false;
            std::vector<std::string> unsafeReasons;

            bool rainEnabled = false;
            bool raining = false; // includes the rain-clear hold-off
            float rainRateMmPerHour = 0.0f;
            bool lensFault = false;

            SensorHealth sensors[SENSOR_COUNT];

            bool environmentValid = false;
            float temperatureC = 0.0f;
            float dewpointC = 0.0f;

            bool skyValid = false;
            float cloudCoverPercent = 0.0f;

            // From the sun's position; nightKnown is false without a clock or
            // a location, in which case night-only rules don't hold alerts back.
            bool nightKnown = false;
            bool isNight = false;
        };

        // Edge-triggered, rate-limited event detection. Each condition is a
        // tracked boolean; a notification is emitted when its value differs
        // from the last *notified* value and the per-condition cooldown has
        // passed. A change suppressed by the cooldown is not lost - it's
        // sent once the cooldown ends if the condition still differs, so the
        // last notification always matches reality.
        class AlertEngine
        {
        public:
            std::vector<Alert> update(const AlertInputs &inputs, const AlertRules &rules);

        private:
            struct Tracker
            {
                bool initialized = false;
                bool notified = false;
                bool hasNotified = false;
                uint32_t lastNotifiedAt = 0;
                bool pending = false; // value differs from notified, waiting to settle
                uint32_t pendingSince = 0;
            };

            // Returns true when a transition to `current` should be emitted now.
            // `settle`: seconds `current` must hold before it counts.
            static bool sync(Tracker &tracker, bool current, uint32_t now, uint32_t cooldown, bool emitAllowed, uint32_t settle = 0);

            bool started = false;
            uint32_t startedAt = 0;

            Tracker safety;
            Tracker rain;
            Tracker lens;
            Tracker dew;
            Tracker sky; // notified value = "clear"
            Tracker sensors[SENSOR_COUNT];

            // Observed (hysteresis) state for threshold conditions.
            bool dewObserved = false;
            bool skyClear = false;
        };

        // Formatting helpers shared with the dispatcher's test alerts.
        std::string joinReasons(const std::vector<std::string> &reasons);

    } // namespace Alerts
} // namespace SQM
