#include "DeviceCore.h"
#include "DeviceCoreInternal.h"

#include <cmath>

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

    void readSkySensors(Readings::Snapshot &r, const SensorSnapshot &snapshot, uint32_t now, uint32_t staleAfter)
    {
        r.source = snapshot.skySource;
        for (size_t i = 0; i < SKY_ROLE_COUNT; ++i)
        {
            const SkySensorSnapshot &sky = snapshot.skySensors[i];
            Readings::SkySensorEntry &e = r.skySensors[i];
            e.present = sky.enabled;
            if (!e.present)
                continue;
            const TSL2591Reading &reading = sky.reading;
            const TSL2591Diagnostics &diag = sky.diagnostics;
            e.status = readingStatus(reading.status, sky.initialized, sky.lastUpdate, now, staleAfter);
            e.ageMs = ageMs(now, reading.timestamp);
            e.sqm = sky.metrics.sqm;
            e.rawSqm = reading.rawSqm;
            e.nelm = sky.metrics.nelm;
            e.bortle = static_cast<int>(std::lround(sky.metrics.bortle));
            e.description = SkyQuality::getBortleDescription(sky.metrics.bortle);
            e.calibrated = diag.calibrated;
            e.averagingWindowSeconds = diag.averagingWindowSeconds;
            e.lux = reading.lux;
            e.gain = diag.gainName != nullptr ? diag.gainName : "";
            e.integrationMs = diag.integrationMs;
            e.saturated = diag.saturated;
            e.nightMode = diag.nightMode;
        }
    }
} // namespace SQM::Core::Detail

namespace SQM::Core
{
    void writeSkySensorStatus(JsonObject root, const SensorSnapshot &snapshot, const Readings::Snapshot &readings)
    {
        JsonObject sensors;
        for (size_t i = 0; i < SKY_ROLE_COUNT; ++i)
        {
            const SkySensorSnapshot &sky = snapshot.skySensors[i];
            if (!sky.enabled)
                continue;
            if (sensors.isNull())
                sensors = root.createNestedObject("skySensors");
            const Readings::SkySensorEntry &entry = readings.skySensors[i];
            JsonObject out = sensors.createNestedObject(skyRoleName(static_cast<SkyRole>(i)));
            out["status"] = Readings::statusName(entry.status);
            if (entry.status != Readings::Status::Missing)
                out["ageMs"] = entry.ageMs;
            // Found and answering on its bus; a stale reading still means it's there.
            const bool detected = entry.status == Readings::Status::Ok || entry.status == Readings::Status::Stale;
            out["detected"] = detected;
            if (!detected)
                continue;
            const TSL2591Diagnostics &diag = sky.diagnostics;
            out["gain"] = diag.gainName != nullptr ? diag.gainName : "";
            out["integrationMs"] = diag.integrationMs;
            out["saturated"] = diag.saturated;
            out["nightMode"] = diag.nightMode;
            out["sampleCount"] = diag.sampleCount;
            out["windowSamples"] = windowSamples(diag);
            out["darkVisibleOffset"] = diag.darkVisibleOffset;
        }
    }
} // namespace SQM::Core
