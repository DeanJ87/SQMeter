#include "BatteryLogic.h"
#include "DeviceCoreInternal.h"

#include <algorithm>
#include <cstdio>

// The battery (specs/032-battery): presence, estimates and what every
// interface reports (contracts/readings.md).

namespace SQM::Core
{
    namespace
    {
        // The gauge is read every 5 s (or the power-saving interval), not at
        // the sensor read interval.
        uint32_t batteryStaleAfter(const BatterySnapshot &battery, const Config &cfg)
        {
            const uint32_t gauge =
                battery.power == Battery::PowerState::Normal ? Battery::READ_INTERVAL_MS : cfg.battery.saveReadIntervalSeconds * 1000U;
            return std::max(cfg.sensor.readIntervalMs, gauge) + SENSOR_STALE_GRACE_MS;
        }
    } // namespace

    namespace Detail
    {
        void deriveBattery(BatterySnapshot &battery, const Config &cfg)
        {
            battery.enabled = cfg.battery.enabled;
            // Monitoring off: reported as if no gauge were fitted (research D8).
            battery.present = battery.enabled && (battery.detected || cfg.battery.chargerPins());
            const BatteryReading &r = battery.reading;
            Battery::EstimateInput in;
            in.valid = battery.enabled && battery.initialized && r.status == SensorStatus::Ok && !r.estimating;
            in.percent = r.percent;
            in.rateValid = r.rateValid;
            in.rate = r.ratePercentPerHour;
            in.state = r.chargeState;
            in.capacityMah = cfg.battery.capacityMah;
            const Battery::Estimate estimate = Battery::estimate(in);
            battery.timeToEmptyMinutes = estimate.timeToEmptyMinutes;
            battery.timeToFullMinutes = estimate.timeToFullMinutes;
            battery.currentMa = estimate.currentMa;
        }

        Readings::Battery readBattery(const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
        {
            const BatterySnapshot &b = snapshot.battery;
            Readings::Battery out;
            out.present = b.present;
            if (!b.present)
                return out;
            const BatteryReading &r = b.reading;
            out.status = readingStatus(r.status, b.initialized, b.lastUpdate, now, batteryStaleAfter(b, cfg));
            out.ageMs = ageMs(now, r.timestamp);
            out.chargerPins = cfg.battery.chargerPins();
            out.estimating = r.estimating;
            if (!r.estimating)
                out.percent = r.percent;
            out.voltage = r.voltage;
            if (r.rateValid)
                out.ratePercentPerHour = r.ratePercentPerHour;
            out.state = Battery::chargeStateName(r.chargeState);
            out.usbPower = r.usbPower;
            out.fault = Battery::chargeFaultName(r.fault);
            out.timeToEmptyMinutes = b.timeToEmptyMinutes;
            out.timeToFullMinutes = b.timeToFullMinutes;
            out.powerSaving = b.power != Battery::PowerState::Normal;
            return out;
        }

        void batterySafetyInputs(Alpaca::SafetyInputs &in, const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
        {
            const BatterySnapshot &b = snapshot.battery;
            const BatteryReading &r = b.reading;
            // Monitoring off counts as no data: the rule, if on, fails safe.
            const bool fresh = b.lastUpdate != 0 && now - b.lastUpdate <= cfg.alpaca.staleAfterSeconds * 1000U;
            in.batteryValid = b.present && b.initialized && r.status == SensorStatus::Ok && !r.estimating && fresh;
            in.batteryPercent = r.percent;
            in.shutdownPending = b.present && b.power == Battery::PowerState::ShutdownPending;
            in.shutdownPercent = cfg.battery.shutdownPercent;
        }

        void batteryAlertInputs(Alerts::BatteryInputs &in, const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
        {
            const BatterySnapshot &b = snapshot.battery;
            const BatteryReading &r = b.reading;
            const Readings::Status status = readingStatus(r.status, b.initialized, b.lastUpdate, now, batteryStaleAfter(b, cfg));
            in.present = b.present;
            in.valid = b.present && status == Readings::Status::Ok && !r.estimating;
            in.percent = r.percent;
            in.charged = b.present && r.chargeState == Battery::ChargeState::Charged;
            in.fault = b.present && r.chargeState == Battery::ChargeState::Fault;
        }

        uint32_t batteryAlarmFlag(Alerts::AlertType type)
        {
            static_assert(Alerts::BATTERY_RULE_FLAGS == (Alpaca::UnsafeBatteryLow | Alpaca::UnsafeBatteryData), "the rule's bits");
            switch (type)
            {
            case Alerts::AlertType::BatteryLow:
            case Alerts::AlertType::BatteryCritical:
                return Alpaca::UnsafeBatteryLow;
            case Alerts::AlertType::BatteryShutdown:
                return Alpaca::UnsafeShutdown;
            default:
                return 0;
            }
        }
    } // namespace Detail

    Readings::Battery batteryReadings(const SensorSnapshot &snapshot, const Config &cfg, uint32_t nowMs)
    {
        return Detail::readBattery(snapshot, cfg, nowMs);
    }

    void writeBatteryStatus(JsonObject root, const SensorSnapshot &snapshot, const Readings::Snapshot &readings)
    {
        JsonObject battery = Readings::writeBattery(root, readings.battery);
        if (readings.battery.present && snapshot.battery.power == Battery::PowerState::ShutdownPending)
            battery["shutdownInSeconds"] = snapshot.battery.shutdownInSeconds;
    }

    void writeBatteryDiagnostics(JsonObject root, const SensorSnapshot &snapshot, const Config &cfg)
    {
        const BatterySnapshot &b = snapshot.battery;
        if (!b.detected)
            return;
        const BatteryReading &r = b.reading;
        JsonObject out = root.createNestedObject("battery");
        char version[8];
        std::snprintf(version, sizeof version, "0x%04X", static_cast<unsigned>(r.version));
        out["version"] = version; // copied
        out["vcellRaw"] = r.vcellRaw;
        out["socRaw"] = r.socRaw;
        out["crateRaw"] = r.crateRaw;
        out["statusRaw"] = r.statusRaw;
        out["resetSeen"] = r.resetSeen;
        out["estimating"] = r.estimating;
        if (cfg.battery.chargerPins())
        {
            JsonObject charger = out.createNestedObject("charger");
            charger["stat1"] = r.stat1;
            charger["stat2"] = r.stat2;
            charger["pg"] = r.pg;
        }
        else
            out["charger"] = nullptr;
        if (r.gaugeAlert)
            out["gaugeAlert"] = *r.gaugeAlert;
        else
            out["gaugeAlert"] = nullptr;
        out["policy"] = Battery::powerStateName(b.power);
        if (b.currentMa)
            out["currentMa"] = static_cast<int>(*b.currentMa + (*b.currentMa < 0 ? -0.5f : 0.5f));
        else
            out["currentMa"] = nullptr;
    }
} // namespace SQM::Core
