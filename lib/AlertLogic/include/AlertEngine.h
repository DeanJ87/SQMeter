#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
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
            AlertsOn,     // alerts resumed, with the current verdict
            Test,
            // The imaging app (any Alpaca client) stopped checking a device,
            // came back, or disconnected normally (specs/021).
            ClientLost,
            ClientBack,
            ClientDisconnected,
        };
        // Keep in step with the enum: alertTypeName() is a table indexed by it.
        constexpr size_t ALERT_TYPE_COUNT = static_cast<size_t>(AlertType::ClientDisconnected) + 1;

        // How loudly an alert is delivered; set per event in the alert
        // settings. The dispatcher maps it to each channel's priority, and
        // Wake also rings paired phones over Bluetooth.
        enum class AlertLevel : uint8_t
        {
            Off = 0,
            Quiet = 1,
            Normal = 2,
            Urgent = 3,
            Wake = 4,
        };

        struct Alert
        {
            AlertType type = AlertType::Test;
            AlertLevel level = AlertLevel::Normal;
            std::string title;
            std::string message;
            std::string sound; // Pushover sound; empty = channel default
            // Values for message templates ({name} -> value), e.g. "reasons",
            // "sensor", "cloud". The firmware adds readings and settings.
            std::vector<std::pair<std::string, std::string>> vars;
            // Other events sent in this same notification (see stackAlerts).
            std::vector<AlertType> stacked;
        };

        const char *alertTypeName(AlertType type);
        const char *alertLevelName(AlertLevel level);

        // How each level is delivered. Pushover: quiet -1 (no sound), normal
        // 0, urgent 1 (bypasses quiet hours), wake 2 (emergency: repeats until
        // acknowledged, so it needs retry/expire). ntfy: low/default/high/max.
        int pushoverPriority(AlertLevel level);
        const char *ntfyPriority(AlertLevel level);

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
            // Safety changes only while it's dark too: dawn brightening the
            // sky past the SQM limit mustn't page anyone. At nightfall the
            // verdict is compared with what was last announced.
            bool safetyNightOnly = true;
            uint32_t cooldownSeconds = 300;
            // A sensor fault (or recovery) must hold this long before it's
            // announced, so blips - saving settings, reconfiguring a sensor,
            // an OTA upload stalling the loop - don't page anyone.
            uint32_t sensorSettleSeconds = 30;
            // Nothing is sent this soon after boot; state is still tracked,
            // so a reboot doesn't announce whatever the startup state is.
            // Imaging-app events don't wait for it: they need a request
            // since the restart anyway.
            uint32_t startupGraceSeconds = 60;
            bool onClientLost = true;
            bool onClientBack = true;
            bool onClientDisconnected = false;
        };

        constexpr size_t SENSOR_COUNT = 5;
        constexpr size_t CLIENT_DEVICE_COUNT = 2; // SafetyMonitor, ObservingConditions

        // One Alpaca device as the imaging app sees it (from Alpaca::ClientWatch).
        struct ClientInputs
        {
            const char *device = "";      // "safety monitor" / "weather device"
            bool watching = false;        // a session since the restart
            bool silent = false;          // no request for the silence time
            bool disconnectedNow = false; // disconnected cleanly since the last pass
            std::string silentFor;        // the silence time, e.g. "2 min"
            std::string lastChecked;      // "21:04", or "3 min ago" without a clock
            std::string clientId;         // last Alpaca ClientID, or ""
        };

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
            // The verdict isn't a real one yet: no sensor data since boot,
            // or unsafe only because the safe delay hasn't run out (after a
            // restart the delay runs from boot; mid-run it's the tail of an
            // unsafe spell already announced). Treated as no news.
            bool safetySettling = false;
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

            ClientInputs clients[CLIENT_DEVICE_COUNT];
        };

        // Why a change hasn't been announced yet.
        enum class WaitKind : uint8_t
        {
            Grace,    // nothing is sent this soon after boot
            Settle,   // the new state must hold this long first
            Cooldown, // the last notice for this condition was too recent
        };

        struct Wait
        {
            std::string condition; // "safety", "rain", "lens", "dew", "sky", "sensor:<name>", "startup"
            WaitKind kind = WaitKind::Settle;
            uint32_t remainingSeconds = 0;
        };

        const char *waitKindName(WaitKind kind);

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

            // What the user was last told before a restart (true = unsafe).
            // After the startup grace the verdict is compared with this, so
            // a change across a restart is announced and a restart that
            // changes nothing isn't. Call before the first update().
            void seedSafety(bool unsafe);

            // Changes seen but not announced yet, and how long until they
            // can be (as of the last update()). Doesn't change any state.
            std::vector<Wait> waits(uint32_t nowSeconds, const AlertRules &rules) const;

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
            bool safetySeeded = false;
            Tracker rain;
            Tracker lens;
            Tracker dew;
            Tracker sky; // notified value = "clear"
            Tracker sensors[SENSOR_COUNT];
            const char *sensorNames[SENSOR_COUNT] = {};

            // Observed (hysteresis) state for threshold conditions.
            bool dewObserved = false;
            bool skyClear = false;

            // Imaging app: notified value = "lost"; disconnects wait out the
            // cooldown like any other change.
            Tracker clients[CLIENT_DEVICE_COUNT];
            bool lostAnnounced[CLIENT_DEVICE_COUNT] = {}; // a "stopped checking" awaiting its "is back"
            bool disconnectPending[CLIENT_DEVICE_COUNT] = {};
            bool disconnectSent = false;
            uint32_t disconnectSentAt = 0;

            enum class ClientChange
            {
                None,
                Lost,
                Back
            };
            ClientChange clientSilence(size_t index, bool silent, bool allowed, uint32_t now, uint32_t cooldown);
            void updateClients(const AlertInputs &inputs, const AlertRules &rules, std::vector<Alert> &alerts);
        };

        // Formatting helpers shared with the dispatcher's test alerts.
        // One reason per line, bulleted.
        std::string joinReasons(const std::vector<std::string> &reasons);

        // Replaces each {name} in `text` with its value from `vars`. Unknown
        // names are left as they are, so a typo shows up in the alert.
        std::string renderTemplate(const std::string &text, const std::vector<std::pair<std::string, std::string>> &vars);

        // Several alerts raised at once become one notification: the
        // loudest one leads (its type, level and sound), titles are joined
        // with " · " and messages one after another.
        Alert stackAlerts(const std::vector<Alert> &alerts);

    } // namespace Alerts
} // namespace SQM
