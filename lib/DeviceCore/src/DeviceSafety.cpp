#include "DeviceCore.h"

// The safety verdict's limits from the settings (lib/AlpacaLogic SafetyEvaluator).

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
        t.windSpeedUnsafeEnabled = cfg.alpaca.windSpeedUnsafeEnabled;
        t.windSpeedUnsafeMs = cfg.alpaca.windSpeedUnsafeMs;
        t.windGustUnsafeEnabled = cfg.alpaca.windGustUnsafeEnabled;
        t.windGustUnsafeMs = cfg.alpaca.windGustUnsafeMs;
        t.batteryEnabled = cfg.alpaca.batteryEnabled;
        t.batteryMinPercent = cfg.alpaca.batteryMinPercent;
        return t;
    }
} // namespace SQM::Core
