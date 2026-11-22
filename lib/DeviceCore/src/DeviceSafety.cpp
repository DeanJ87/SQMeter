#include "DeviceCore.h"
#include "DeviceCoreInternal.h"

// The safety verdict's limits from the settings (lib/AlpacaLogic SafetyEvaluator),
// its status after the delays, and the safety document.

namespace SQM::Core
{
    Alpaca::SafetyThresholds safetyThresholds(const Config &cfg)
    {
        Alpaca::SafetyThresholds t;
        t.manualOverrideUnsafe = cfg.alpaca.manualOverrideUnsafe;
        t.staleAfterSeconds = cfg.alpaca.staleAfterSeconds;
        t.cloudCoverEnabled = cfg.alpaca.cloudCoverEnabled;
        t.cloudCoverUnsafePercent = cfg.alpaca.cloudCoverUnsafePercent;
        t.sqmMinEnabled = cfg.alpaca.sqmMinEnabled;
        t.sqmMinSafe = cfg.alpaca.sqmMinSafe;
        t.humidityMaxEnabled = cfg.alpaca.humidityMaxEnabled;
        t.humidityMaxSafe = cfg.alpaca.humidityMaxSafe;
        t.dewpointMarginEnabled = cfg.alpaca.dewpointMarginEnabled;
        t.dewpointMarginMinC = cfg.alpaca.dewpointMarginMinC;
        t.rainUnsafeEnabled = cfg.alpaca.rainUnsafeEnabled;
        t.rainSensorRequired = cfg.alpaca.rainSensorRequired;
        t.rainNeedsCloud = cfg.alpaca.rainNeedsCloud;
        t.rainNeedsCloudPercent = cfg.alpaca.rainNeedsCloudPercent;
        t.windSpeedUnsafeEnabled = cfg.alpaca.windSpeedUnsafeEnabled;
        t.windSpeedUnsafeMs = cfg.alpaca.windSpeedUnsafeMs;
        t.windGustUnsafeEnabled = cfg.alpaca.windGustUnsafeEnabled;
        t.windGustUnsafeMs = cfg.alpaca.windGustUnsafeMs;
        t.batteryEnabled = cfg.alpaca.batteryEnabled;
        t.batteryMinPercent = cfg.alpaca.batteryMinPercent;
        return t;
    }

    bool updateSafety(
        SafetyStatus &status, Alpaca::SafeDelayFilter &filter, const Alpaca::SafetyResult &result, const Config &cfg, uint32_t now)
    {
        const bool reportedSafe =
            filter.update(result.isSafe, now, cfg.alpaca.safeDelaySeconds, cfg.alpaca.unsafeDelaySeconds, result.reasonFlags);
        const bool changed = reportedSafe != status.isSafe || status.evaluatedAtMs == 0;
        if (changed)
            status.changedAtMs = now;
        status.isSafe = reportedSafe;
        status.rawSafe = result.isSafe;
        status.reasonFlags = result.reasonFlags;
        status.reasons = result.unsafeReasons;
        status.secondsUntilSafe = filter.secondsUntilSafe();
        status.secondsUntilUnsafe = filter.secondsUntilUnsafe();
        status.evaluatedAtMs = now;
        if (result.dewSuspected && !status.dewSuspected)
            status.dewSinceMs = now;
        status.dewSuspected = result.dewSuspected;
        status.dewRainRateMmPerHour = result.rainRateMmPerHour;
        status.dewCloudCoverPercent = result.cloudCoverPercent;
        return changed;
    }

    void writeSafety(JsonObject target, const SafetyStatus &status, const Config &cfg, uint32_t now, const HeldBy *heldBy)
    {
        target["safe"] = status.isSafe;
        target["rawSafe"] = status.rawSafe;
        target["alpacaEnabled"] = cfg.alpaca.enabled;
        target["reasonFlags"] = status.reasonFlags;
        JsonArray reasons = target.createNestedArray("reasons");
        for (const std::string &reason : status.reasons)
            reasons.add(reason);
        target["secondsUntilSafe"] = status.secondsUntilSafe;
        target["secondsUntilUnsafe"] = status.secondsUntilUnsafe;
        // Rules switched on but ignored because their sensor is off
        // (specs/020-settings-dependencies FR-009).
        JsonArray notInEffect = target.createNestedArray("rulesNotInEffect");
        for (const std::string &rule : Deps::rulesNotInEffect(cfg))
            notInEffect.add(rule);
        target["evaluatedAgeMs"] = ageMs(now, status.evaluatedAtMs);
        target["changedAgeMs"] = ageMs(now, status.changedAtMs);
        // Rain needs cloud (spec 036): a flag, never a reason.
        target["dewSuspected"] = status.dewSuspected;
        if (status.dewSuspected)
        {
            JsonObject dew = target.createNestedObject("dew");
            dew["rainIntensity"] = Detail::roundTo(status.dewRainRateMmPerHour, 2);
            dew["cloudCoverPercent"] = Detail::roundTo(status.dewCloudCoverPercent, 0);
            dew["sinceAgeMs"] = ageMs(now, status.dewSinceMs);
        }
        JsonArray holdList = target.createNestedArray("holds");
        if (heldBy != nullptr)
            heldBy->holds.writeJson(holdList, now, heldBy->epoch);
    }
} // namespace SQM::Core

namespace SQM::Core::Detail
{
    // Rain needs cloud (spec 036 FR-006): only an IR sky reading that is
    // found, OK, fresh and gives a cloud cover may overrule the rain sensor.
    // A NaN sky or air temperature reaches the model as a NaN delta, which it can't class.
    bool skyCloudUsable(const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
    {
        const Readings::Status ir =
            readingStatus(snapshot.mlx.status, snapshot.mlxInitialized, snapshot.mlxLastUpdate, now, staleAfterMs(snapshot, cfg));
        const CloudMetrics &cloud = snapshot.cloud;
        return ir == Readings::Status::Ok && cloud.condition != CloudCondition::UNKNOWN && std::isfinite(cloud.correctedDelta) &&
               std::isfinite(cloud.cloudCoverPercent);
    }
} // namespace SQM::Core::Detail
