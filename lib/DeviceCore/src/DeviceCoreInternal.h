#pragma once

// Helpers shared by the DeviceCore sources (DeviceCore.cpp, DeviceSky.cpp).

#include "DeviceCore.h"

namespace SQM::Core::Detail
{
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

    // readings sky.source and skySensors (spec 030 FR-008).
    void readSkySensors(Readings::Snapshot &r, const SensorSnapshot &snapshot, uint32_t now, uint32_t staleAfter);
} // namespace SQM::Core::Detail
