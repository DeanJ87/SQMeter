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
            // The battery (specs/032). BatteryRule is the safety rule's own
            // alert (always Urgent, not an event setting).
            BatteryLow,
            BatteryCritical,
            BatteryShutdown, // going to sleep: battery empty
            PowerRestored,
            ChargeComplete,
            ChargeFault,
            BatteryRule,
        };
        // Keep in step with the enum: alertTypeName() is a table indexed by it.
        constexpr size_t ALERT_TYPE_COUNT = static_cast<size_t>(AlertType::BatteryRule) + 1;

        // The safety verdict's battery rule bits (Alpaca::UnsafeBatteryLow,
        // UnsafeBatteryData; DeviceCore checks they match).
        constexpr uint32_t BATTERY_RULE_FLAGS = (1u << 14) | (1u << 15);
        // A battery level alert fires again only once the charge is back this
        // far above its threshold.
        constexpr float BATTERY_HYSTERESIS_PERCENT = 5.0f;

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
            // Battery (specs/032).
            bool onBatteryLow = true;
            bool onBatteryCritical = true;
            bool onBatteryShutdown = true;
            bool onPowerRestored = false;
            bool onChargeComplete = false;
            bool onChargeFault = true;
            bool onBatteryRule = false; // the rule's alert or phone alarm
            float batteryLowPercent = 20.0f;
            float batteryCriticalPercent = 10.0f;
            float batteryShutdownPercent = 5.0f;
        };

        // Light (or SQM), IR sky, environment, rain, anemometer, SQM-L (spec 030).
        constexpr size_t SENSOR_COUNT = 6;
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

        // The battery as the alert engine sees it (specs/032).
        struct BatteryInputs
        {
            bool present = false;
            bool valid = false; // a percent: ok, not estimating
            float percent = 0.0f;
            bool charged = false; // the charger reports full
            bool fault = false;   // the charger reports a fault
            // One-shots from the power policy: sent at once, exempt from the
            // startup grace and the cooldown.
            bool shutdownStarted = false;
            bool powerRestored = false; // USB power is back
            bool recovered = false;     // power saving ended: the charge recovered without USB
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
            uint32_t unsafeFlags = 0; // the verdict's reason bits

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

            BatteryInputs battery;
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

            // When a change may be announced: the time, the cooldown since the
            // last announcement, whether announcing is allowed at all, and
            // `settle`, the seconds a new value must hold before it counts.
            struct Emit
            {
                uint32_t now;
                uint32_t cooldown;
                bool allowed;
                uint32_t settle = 0;
            };

            // Returns true when a transition to `current` should be emitted now.
            // A tracker that follows the condition silently (before its first
            // update, during the startup grace, while its rule is off) takes the
            // current value, or with `announceAtStart` the all-clear value, so a
            // condition already present then (a battery already low at power-on)
            // is announced once announcing is allowed.
            static bool sync(Tracker &tracker, bool current, const Emit &emit, bool announceAtStart = false);

            // One update(): the time, the cooldown and whether the startup
            // grace is over, shared by every condition.
            struct Step
            {
                uint32_t now;
                uint32_t cooldown;
                bool pastGrace;
                Emit emit(bool enabled, uint32_t settle = 0) const { return {now, cooldown, pastGrace && enabled, settle}; }
            };
            void updateSafety(const AlertInputs &inputs, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);
            void updateRain(const AlertInputs &inputs, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);
            void updateSensors(const AlertInputs &inputs, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);
            void updateDew(const AlertInputs &inputs, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);
            void updateSky(const AlertInputs &inputs, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);
            void updateBattery(const AlertInputs &inputs, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);
            static void batteryOneShots(const BatteryInputs &battery, const AlertRules &rules, std::vector<Alert> &alerts);
            void batteryLevels(const BatteryInputs &battery, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);
            void batteryRule(const AlertInputs &inputs, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts);

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

            // Battery: below low / critical (with hysteresis), charged, fault.
            Tracker batteryLow;
            Tracker batteryCritical;
            Tracker chargeComplete;
            Tracker chargeFault;
            bool belowLow = false;
            bool belowCritical = false;
            uint32_t batteryRuleFlags = 0; // the rule bits last seen
            bool batteryRuleSent = false;
            uint32_t batteryRuleSentAt = 0; // a flapping rule alerts at most once per cooldown

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
