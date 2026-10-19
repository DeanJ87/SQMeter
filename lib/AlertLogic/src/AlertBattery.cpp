#include "AlertEngine.h"

#include <algorithm>
#include <cstdio>

// The battery's alert events (specs/032-battery, contracts/integrations.md).
// The rest of the engine is in AlertEngine.cpp.

namespace SQM
{
    namespace Alerts
    {
        namespace
        {
            Alert make(AlertType type, std::string title, std::string message)
            {
                Alert alert;
                alert.type = type;
                alert.title = std::move(title);
                alert.message = std::move(message);
                return alert;
            }

            std::string format(const char *fmt, double a, double b)
            {
                char buffer[160];
                std::snprintf(buffer, sizeof(buffer), fmt, a, b);
                return buffer;
            }
        } // namespace

        // The battery (specs/032, contracts/integrations.md): low and critical
        // once per crossing, re-armed BATTERY_HYSTERESIS_PERCENT above; charge
        // complete and charge fault on entering; going to sleep and power
        // restored at once; the safety rule's alert when its bits appear.
        // Low, critical and a charge fault already present at power-on (or
        // when the gauge's first percent arrives) are announced once the
        // startup grace ends.
        void AlertEngine::updateBattery(const AlertInputs &in, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts)
        {
            const BatteryInputs &b = in.battery;
            if (b.present)
            {
                batteryOneShots(b, rules, alerts);
                if (b.valid)
                    batteryLevels(b, rules, step, alerts);
                if (sync(chargeComplete, b.charged, step.emit(rules.onChargeComplete)) && b.charged)
                    alerts.push_back(make(AlertType::ChargeComplete, "Battery charged", "The battery is fully charged."));
                if (sync(chargeFault, b.fault, step.emit(rules.onChargeFault), true) && b.fault)
                    alerts.push_back(make(
                        AlertType::ChargeFault, "Charging fault", "The charger stopped: too cold or too hot to charge, or a bad cell."));
            }
            batteryRule(in, rules, step, alerts);
        }

        // Going to sleep and power restored: sent at once, one-shot by construction.
        void AlertEngine::batteryOneShots(const BatteryInputs &b, const AlertRules &rules, std::vector<Alert> &alerts)
        {
            if (b.shutdownStarted && rules.onBatteryShutdown)
                alerts.push_back(make(
                    AlertType::BatteryShutdown,
                    "Going to sleep: battery empty",
                    format(
                        "Battery at %.0f%% (sleep below %.0f%%). The device sleeps until USB power returns.",
                        b.percent,
                        rules.batteryShutdownPercent)));
            if (b.powerRestored && rules.onPowerRestored)
                alerts.push_back(make(AlertType::PowerRestored, "Power restored", "USB power is back; the battery is charging."));
            if (b.recovered && rules.onPowerRestored)
                alerts.push_back(make(
                    AlertType::PowerRestored,
                    "Battery recovered",
                    format(
                        "Battery back at %.0f%%: power saving is off (it starts below %.0f%%).", b.percent, rules.batteryCriticalPercent)));
        }

        // Low and critical, once per crossing (with hysteresis).
        void AlertEngine::batteryLevels(const BatteryInputs &b, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts)
        {
            const float low = rules.batteryLowPercent;
            const float critical = rules.batteryCriticalPercent;
            belowLow = b.percent < low + (belowLow ? BATTERY_HYSTERESIS_PERCENT : 0.0f);
            belowCritical = b.percent < critical + (belowCritical ? BATTERY_HYSTERESIS_PERCENT : 0.0f);
            if (sync(batteryLow, belowLow, step.emit(rules.onBatteryLow), true) && belowLow)
                alerts.push_back(
                    make(AlertType::BatteryLow, "Battery low", format("Battery at %.0f%% (low below %.0f%%).", b.percent, low)));
            if (sync(batteryCritical, belowCritical, step.emit(rules.onBatteryCritical), true) && belowCritical)
                alerts.push_back(make(
                    AlertType::BatteryCritical,
                    "Battery critical",
                    format("Battery at %.0f%% (critical below %.0f%%).", b.percent, critical)));
        }

        // The battery safety rule: when one of its reasons newly appears, at
        // most once per cooldown (data that comes and goes on a glitchy bus
        // flips its reason between low and missing; constitution I: no storms).
        void AlertEngine::batteryRule(const AlertInputs &in, const AlertRules &rules, const Step &step, std::vector<Alert> &alerts)
        {
            const uint32_t ruleBits = in.safetyKnown ? (in.unsafeFlags & BATTERY_RULE_FLAGS) : 0;
            const uint32_t appeared = ruleBits & ~batteryRuleFlags;
            batteryRuleFlags = ruleBits;
            if (appeared == 0 || !rules.onBatteryRule || !step.pastGrace)
                return;
            if (batteryRuleSent && step.now - batteryRuleSentAt < step.cooldown)
                return;
            batteryRuleSent = true;
            batteryRuleSentAt = step.now;
            const auto reason = std::find_if(
                in.unsafeReasons.begin(), in.unsafeReasons.end(), [](const std::string &r) { return r.rfind("Battery", 0) == 0; });
            Alert alert =
                make(AlertType::BatteryRule, "Battery: unsafe", reason != in.unsafeReasons.end() ? *reason : "Battery data missing");
            alert.level = AlertLevel::Urgent;
            alerts.push_back(std::move(alert));
        }

    } // namespace Alerts
} // namespace SQM
