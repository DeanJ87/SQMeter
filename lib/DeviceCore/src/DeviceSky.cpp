#include "DeviceCore.h"
#include "DeviceCoreInternal.h"

// The two sky sensors (specs/030-dual-sky-sensors): the authoritative copy
// and the per-sensor documents.

namespace SQM::Core::Detail
{
    // Spec 030 research D3: the authoritative role's reading, or an
    // empty not-ok copy when that sensor is off. No fallback.
    void deriveSky(SensorSnapshot &snapshot, const Config &cfg)
    {
        for (size_t i = 0; i < SKY_ROLE_COUNT; ++i)
        {
            SkySensorSnapshot &sky = snapshot.skySensors[i];
            sky.enabled = cfg.skyEnabled(static_cast<SkyRole>(i));
            sky.metrics = sky.enabled ? SkyQuality::calculate(sky.reading.lux) : SkyQualityMetrics{};
        }
        snapshot.skySource = cfg.sky.authoritative;
        const SkySensorSnapshot &source = snapshot.skySensors[skyRoleIndex(snapshot.skySource)];
        if (source.enabled)
        {
            snapshot.tsl = source.reading;
            snapshot.tslDiagnostics = source.diagnostics;
            snapshot.tslInitialized = source.initialized;
            snapshot.tslLastUpdate = source.lastUpdate;
            snapshot.sky = source.metrics;
            return;
        }
        snapshot.tsl = TSL2591Reading{};
        snapshot.tslDiagnostics = TSL2591Diagnostics{};
        snapshot.tslInitialized = false;
        snapshot.tslLastUpdate = 0;
        snapshot.sky = SkyQualityMetrics{};
    }
} // namespace SQM::Core::Detail
