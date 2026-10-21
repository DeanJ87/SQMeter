#pragma once

// Helpers shared by the DeviceCore sources (DeviceCore.cpp, DeviceSky.cpp).

#include "DeviceCore.h"

namespace SQM::Core::Detail
{
    // A reading is stale this long after the read interval in use (power
    // saving reads less often, spec 032).
    inline uint32_t staleAfterMs(const SensorSnapshot &snapshot, const Config &cfg)
    {
        const uint32_t interval = snapshot.readIntervalMs > cfg.sensor.readIntervalMs ? snapshot.readIntervalMs : cfg.sensor.readIntervalMs;
        return interval + SENSOR_STALE_GRACE_MS;
    }

    // ok unless the driver reports a problem or the last good reading is too old.
    inline Readings::Status readingStatus(SensorStatus status, bool initialized, uint32_t lastUpdate, uint32_t now, uint32_t staleAfter)
    {
        if (!initialized || status == SensorStatus::NotInitialized)
            return Readings::Status::Missing;
        if (status != SensorStatus::Ok)
            return Readings::Status::Error;
        if (lastUpdate == 0 || now - lastUpdate > staleAfter)
            return Readings::Status::Stale;
        return Readings::Status::Ok;
    }

    // Both sky sensors on: reasons and descriptions name them (spec 030).
    inline bool bothSkySensors(const Config &cfg)
    {
        return cfg.sky.sqmEnabled && cfg.sky.sqmlEnabled;
    }

    // Each sky sensor's metrics and the authoritative copy (spec 030 research D3).
    void deriveSky(SensorSnapshot &snapshot, const Config &cfg);

    // The battery's reported presence and estimates (spec 032, DeviceBattery.cpp).
    void deriveBattery(BatterySnapshot &battery, const Config &cfg);
    Readings::Battery readBattery(const SensorSnapshot &snapshot, const Config &cfg, uint32_t now);
    // The battery rule's inputs and the shutdown reason (spec 032 research D5).
    void batterySafetyInputs(Alpaca::SafetyInputs &in, const SensorSnapshot &snapshot, const Config &cfg, uint32_t now);
    // The battery as the alert engine sees it (one-shots from the power policy are the caller's).
    void batteryAlertInputs(Alerts::BatteryInputs &in, const SensorSnapshot &snapshot, const Config &cfg, uint32_t now);
    // The Bluetooth alarm bit a Wake-level battery event rings phones with.
    uint32_t batteryAlarmFlag(Alerts::AlertType type);

    // readings sky.source and skySensors (spec 030 FR-008).
    void readSkySensors(Readings::Snapshot &r, const SensorSnapshot &snapshot, uint32_t now, uint32_t staleAfter);
} // namespace SQM::Core::Detail
